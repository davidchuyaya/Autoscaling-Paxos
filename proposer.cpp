//
// Created by David Chu on 10/4/20.
//
#include <thread>
#include <algorithm>
#include <google/protobuf/message.h>
#include "proposer.hpp"
#include "utils/config.hpp"
#include "models/message.hpp"

proposer::proposer(const int id) : id(id) {
    findAcceptorGroupIds();
    const std::thread server([&] {startServer(); });
    connectToProposers();
    const std::thread broadcastLeader([&] { broadcastIAmLeader(); });
    const std::thread heartbeatChecker([&] { checkHeartbeats(); });
    std::this_thread::sleep_for(std::chrono::seconds(1)); //TODO loop to see we're connected to F+1 proxy leaders
    mainLoop();
}

void proposer::findAcceptorGroupIds() {
    std::lock_guard<std::mutex> lock(acceptorMutex);
    for (int acceptorGroupId = 0; acceptorGroupId < config::NUM_ACCEPTOR_GROUPS; acceptorGroupId++)
        acceptorGroupIds.emplace_back(acceptorGroupId);
}

[[noreturn]]
void proposer::broadcastIAmLeader() {
    while (true) {
        if (isLeader) {
            time_t t;
            time(&t);
            printf("%d = leader, sending at time: %s\n", id, std::asctime(std::localtime(&t)));
            const ProposerToProposer& iAmLeader = message::createIamLeader();
            {std::lock_guard<std::mutex> lock(proposerMutex);
            for (const int proposerSocket: proposerSockets)
                network::sendPayload(proposerSocket, iAmLeader);}
        }
        std::this_thread::sleep_for(std::chrono::seconds(config::HEARTBEAT_SLEEP_SEC));
    }
}

[[noreturn]]
void proposer::checkHeartbeats() {
    time_t now;
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(config::HEARTBEAT_TIMEOUT_SEC));

        time(&now);

        std::scoped_lock lock(heartbeatMutex, proxyLeaderMutex);
        if (difftime(now, lastLeaderHeartbeat) > config::HEARTBEAT_TIMEOUT_SEC && !isLeader)
            shouldSendScouts = true;

        //remove proxy leaders that have no heartbeat
        auto iterator = proxyLeaders.begin();
        while (iterator != proxyLeaders.end()) {
            const int socket = *iterator;
            if (difftime(now, proxyLeaderHeartbeats[socket]) > config::HEARTBEAT_TIMEOUT_SEC) {
                printf("Proxy leader failed to heartbeat to proposer %d, it is pronounced dead.\n", id);
                //TODO if the timeout is too stringent, we could end up in an invalid program state... we can't remove proxy leaders
                proxyLeaderHeartbeats.erase(socket);

                //resend what was sent to this proxy leader to another. Won't loop forever, assuming at least 1 proxy leader is alive
                //will be inefficient if multiple proxy leaders fail simultaneously, and we just handed off to another failed one.
                int otherProxyLeaderSocket;
                do otherProxyLeaderSocket = fetchNextProxyLeader();
                while (otherProxyLeaderSocket == socket);
                for (const auto& [messageId, message] : proxyLeaderSentMessages[socket])
                    sendToProxyLeader(otherProxyLeaderSocket, message);
                proxyLeaderSentMessages.erase(socket);

                iterator = proxyLeaders.erase(iterator);
            }
            else
                ++iterator;
        }
    }
}

[[noreturn]]
void proposer::startServer() {
    network::startServerAtPort(config::PROPOSER_PORT_START + id, [&](const int clientSocket) {
        //read first incoming message to tell who the connecting node is
        WhoIsThis whoIsThis;
        whoIsThis.ParseFromString(network::receivePayload(clientSocket));
        switch (whoIsThis.sender()) {
            case WhoIsThis_Sender_batcher:
                listenToBatcher(clientSocket);
            case WhoIsThis_Sender_proxyLeader:
                listenToProxyLeader(clientSocket);
            case WhoIsThis_Sender_proposer:
                printf("Server %d connected to proposer\n", id);
                {std::lock_guard<std::mutex> lock(proposerMutex);
                    proposerSockets.emplace_back(clientSocket);}
                listenToProposer(clientSocket);
            default: {}
        }
    });
}

[[noreturn]]
void proposer::listenToBatcher(int socket) {
    BatcherToProposer payload;
    while (true) {
        payload.ParseFromString(network::receivePayload(socket));
        printf("Proposer %d received a batch request\n", id);
        std::lock_guard<std::mutex> lock(unproposedPayloadsMutex);
        unproposedPayloads.insert(unproposedPayloads.end(), payload.requests().begin(), payload.requests().end());

        payload.Clear();
    }
}

[[noreturn]]
void proposer::listenToProxyLeader(int socket) {
    {std::lock_guard<std::mutex> lock(proxyLeaderMutex);
    proxyLeaders.emplace_back(socket);}
    ProxyLeaderToProposer payload;

    while (true) {
        payload.ParseFromString(network::receivePayload(socket));
        if (payload.type() != ProxyLeaderToProposer_Type_heartbeat) {
            std::lock_guard<std::mutex> lock(proxyLeaderMutex);
            proxyLeaderSentMessages[socket].erase(payload.messageid());
        }

        switch (payload.type()) {
            case ProxyLeaderToProposer_Type_p1b:
                handleP1B(payload);
                break;
            case ProxyLeaderToProposer_Type_p2b:
                handleP2B(payload);
                break;
            case ProxyLeaderToProposer_Type_heartbeat: { //store the time we received this heartbeat
                std::lock_guard<std::mutex> lock(heartbeatMutex);
                time(&proxyLeaderHeartbeats[socket]);
                break;
            }
            default: {}
        }
        payload.Clear();
    }
}

void proposer::connectToProposers() {
    //Protocol is "connect to servers with a higher id than yourself, so we don't end up as both server & client for anyone
    for (int i = id + 1; i < config::F + 1; i++) {
        const int proposerPort = config::PROPOSER_PORT_START + i;
        threads.emplace_back(std::thread([&, proposerPort]{
            const int proposerSocket = network::connectToServerAtAddress(config::LOCALHOST, proposerPort);
            network::sendPayload(proposerSocket, message::createWhoIsThis(WhoIsThis_Sender_proposer));
            printf("Proposer %d connected to other proposer\n", id);
            {std::lock_guard<std::mutex> lock(proposerMutex);
                proposerSockets.emplace_back(proposerSocket);}
            listenToProposer(proposerSocket);
        }));
    }
}

[[noreturn]]
void proposer::listenToProposer(const int socket) {
    ProposerToProposer payload;
    while (true) {
        payload.ParseFromString(network::receivePayload(socket));

        {std::lock_guard<std::mutex> lock(heartbeatMutex);
            printf("%d received leader heartbeat for time: %s\n", id,
                   std::asctime(std::localtime(&lastLeaderHeartbeat)));
            time(&lastLeaderHeartbeat);} // store the time we received the heartbeat
        noLongerLeader();
        shouldSendScouts = false; // disable scouts until the leader's heartbeat times out

        payload.Clear();
    }
}

[[noreturn]]
void proposer::mainLoop() {
    while (true) {
        if (shouldSendScouts)
            sendScouts();
        if (isLeader)
            sendCommandersForPayloads();
    }
}

void proposer::sendScouts() {
    // random timeout so a leader is easily elected
    std::this_thread::sleep_for(std::chrono::seconds(id * config::ID_SCOUT_DELAY_MULTIPLIER));
    if (!shouldSendScouts)
        return;

    int currentBallotNum;
    {std::lock_guard<std::mutex> lock(ballotMutex);
        ballotNum += 1;
        currentBallotNum = ballotNum;}
    printf("P1A blasting out: id = %d, ballotNum = %d\n", id, currentBallotNum);

    std::scoped_lock lock(acceptorMutex, proxyLeaderMutex, remainingAcceptorGroupsForScoutsMutex);
    for (const int acceptorGroupId : acceptorGroupIds) {
        remainingAcceptorGroupsForScouts.emplace(acceptorGroupId);
        const ProposerToAcceptor& p1a = message::createP1A(id, currentBallotNum, acceptorGroupId);
        sendToProxyLeader(proxyLeaders[fetchNextProxyLeader()], p1a);
    }
    shouldSendScouts = false;
}

void proposer::handleP1B(const ProxyLeaderToProposer& message) {
    printf("Proposer %d received p1b from acceptor group: %d, committed log length: %d\n", id,
           message.acceptorgroupid(), message.committedlog_size());

    if (message.ballot().id() != id) { //we lost the election
        //store the largest ballot we last saw so we can immediately catch up
        {std::lock_guard lock(ballotMutex);
        ballotNum = message.ballot().ballotnum();}
        noLongerLeader();
        return;
    }

    {std::scoped_lock lock(acceptorGroupLogsMutex, remainingAcceptorGroupsForScoutsMutex);
    acceptorGroupCommittedLogs.emplace_back(message.committedlog().begin(), message.committedlog().end());
    acceptorGroupUncommittedLogs[message.acceptorgroupid()] =
            {message.uncommittedlog().begin(), message.uncommittedlog().end()};
    remainingAcceptorGroupsForScouts.erase(message.acceptorgroupid());

    if (!remainingAcceptorGroupsForScouts.empty()) //we're still waiting for other acceptor groups to respond
        return;}

    //leader election complete
    isLeader = true;
    printf("Proposer %d is leader\n", id);
    const ProposerToProposer& iAmLeader = message::createIamLeader();
    {std::lock_guard<std::mutex> proposerLock(proposerMutex);
    for (const int proposerSocket: proposerSockets)
        network::sendPayload(proposerSocket, iAmLeader);}

    mergeLogs();
}

void proposer::mergeLogs() {
    //TODO all this locking & unlocking breaks up the critical section & makes it unsafe. Consider giant scoped_locks
    std::unique_lock logsLock(acceptorGroupLogsMutex);
    //use tempLog because "log" must be locked. Then we must use a scoped_lock instead, & they don't support unlock()
    const auto& tempLog = Log::mergeCommittedLogs(acceptorGroupCommittedLogs);
    const auto& [uncommittedLog, acceptorGroupForSlot] = Log::mergeUncommittedLogs(acceptorGroupUncommittedLogs);
    acceptorGroupCommittedLogs.clear();
    acceptorGroupUncommittedLogs.clear();
    logsLock.unlock();

    {std::scoped_lock lock(logMutex, unproposedPayloadsMutex);
    log = tempLog; //TODO prevent already committed item from being uncommitted
    for (const auto&[slot, committedPayload] : log)
        //TODO this is O(log.length * unproposedPayloads.length), not great
        unproposedPayloads.erase(std::remove(unproposedPayloads.begin(), unproposedPayloads.end(), committedPayload),unproposedPayloads.end());
    for (const auto&[slot, pValue] : uncommittedLog)
        unproposedPayloads.erase(std::remove(unproposedPayloads.begin(), unproposedPayloads.end(), pValue.payload()),unproposedPayloads.end());}

    if (isLeader) {
        std::scoped_lock locks(ballotMutex, acceptorMutex, uncommittedProposalsMutex, proxyLeaderMutex);

        for (const auto& [slot, pValue] : uncommittedLog) {
            uncommittedProposals[slot] = pValue.payload();
            int acceptorGroup;
            if (acceptorGroupForSlot.find(slot) != acceptorGroupForSlot.end())
                acceptorGroup = acceptorGroupForSlot.at(slot);
            else
                acceptorGroup = acceptorGroupIds[fetchNextAcceptorGroup()];
            sendCommanders(acceptorGroup, slot, pValue.payload());
        }
    }
}

void proposer::sendCommandersForPayloads() {
    {std::lock_guard<std::mutex> lock(unproposedPayloadsMutex);
    if (unproposedPayloads.empty())
        return;}

    //calculate the next unused slot (log is 1-indexed, because 0 = null in protobuf & will be ignored)
    auto nextSlot = 1;
    for (const auto& [slot, payload] : log)
        if (slot >= nextSlot)
            nextSlot = slot + 1;
    for (const auto& [slot, proposal] : uncommittedProposals)
        if (slot >= nextSlot)
            nextSlot = slot + 1;

    std::scoped_lock lock(unproposedPayloadsMutex, ballotMutex, acceptorMutex, proxyLeaderMutex);
    for (const std::string& payload : unproposedPayloads) {
        uncommittedProposals[nextSlot] = payload;
        sendCommanders(fetchNextAcceptorGroup(), nextSlot, payload);
        nextSlot += 1;
    }
    unproposedPayloads.clear();
}

void proposer::sendCommanders(int acceptorGroupId, int slot, const std::string& payload) {
    const ProposerToAcceptor& p2a = message::createP2A(id, ballotNum, slot, payload, acceptorGroupId);
    sendToProxyLeader(proxyLeaders[fetchNextProxyLeader()], p2a);
}

void proposer::handleP2B(const ProxyLeaderToProposer& message) {
    printf("Proposer %d received p2b, highest ballot: [%d, %d]\n", id, message.ballot().id(), message.ballot().ballotnum());

    if (message.ballot().id() != id) { //yikes, we got preempted
        noLongerLeader();
        return;
    }

    // proposal is committed
    std::scoped_lock lock(logMutex, uncommittedProposalsMutex);
    log[message.slot()] = uncommittedProposals[message.slot()];
    uncommittedProposals.erase(message.slot());
}

void proposer::noLongerLeader() {
    printf("Proposer %d is no longer the leader\n", id);
    isLeader = false;
    shouldSendScouts = true;

    {std::lock_guard<std::mutex> lock(proxyLeaderMutex);
    proxyLeaderSentMessages.clear();}

    std::scoped_lock lock(acceptorGroupLogsMutex, remainingAcceptorGroupsForScoutsMutex, unproposedPayloadsMutex);
    acceptorGroupCommittedLogs.clear();
    acceptorGroupUncommittedLogs.clear();
    remainingAcceptorGroupsForScouts.clear();

    std::vector<std::string> unslottedProposals = {};
    unslottedProposals.reserve(uncommittedProposals.size());
    for (const auto& [slot, payload] : uncommittedProposals)
        unslottedProposals.emplace_back(payload);
    unproposedPayloads.insert(unproposedPayloads.begin(), unslottedProposals.begin(), unslottedProposals.end());
    uncommittedProposals.clear();
}

int proposer::fetchNextAcceptorGroup() {
    nextAcceptorGroup = (nextAcceptorGroup + 1) % acceptorGroupIds.size();
    return nextAcceptorGroup;
}

int proposer::fetchNextProxyLeader() {
    nextProxyLeader = (nextProxyLeader + 1) % proxyLeaders.size();
    return nextProxyLeader;
}

void proposer::sendToProxyLeader(const int proxyLeaderSocket, const ProposerToAcceptor& message) {
    proxyLeaderSentMessages[proxyLeaderSocket][message.messageid()] = message;
    network::sendPayload(proxyLeaderSocket, message);
}
