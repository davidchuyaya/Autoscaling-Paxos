//
// Created by David Chu on 10/4/20.
//
#include <thread>
#include <algorithm>
#include <google/protobuf/message.h>
#include "proposer.hpp"
#include "utils/config.hpp"
#include "models/message.hpp"

proposer::proposer(const int id, std::map<int, std::string> proposers, std::map<int, std::map<int, std::string>> acceptors) : id(id) {
    findAcceptorGroupIds(acceptors);
    const std::thread server([&] {startServer(); });
    connectToProposers(proposers);
    const std::thread broadcastLeader([&] { broadcastIAmLeader(); });
    const std::thread heartbeatChecker([&] { checkHeartbeats(); });
    printf("Waiting for proxy leaders\n");

    //wait
    std::unique_lock<std::mutex> lock(proxyLeaderMutex);
    proxyLeaderCV.wait(lock, [&]{return fastProxyLeaders.size() + slowProxyLeaders.size() >= config::F+1;});
    lock.unlock();

    printf("Starting main loop\n");
    mainLoop();
}

void proposer::findAcceptorGroupIds(std::map<int, std::map<int, std::string>> acceptors) {
    std::lock_guard<std::mutex> lock(acceptorMutex);
    for (const auto pair : acceptors)
        acceptorGroupIds.emplace_back(pair.first);
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

        std::scoped_lock lock(heartbeatMutex, proxyLeaderMutex); //TODO time may be out-of-sync due to lock contention?
        if (difftime(now, lastLeaderHeartbeat) > config::HEARTBEAT_TIMEOUT_SEC && !isLeader)
            shouldSendScouts = true;

        //if a proxy leader has no recent heartbeat, move it into the slow list
        auto iterator = fastProxyLeaders.begin();
        while (iterator != fastProxyLeaders.end()) {
            const int socket = *iterator;
            if (difftime(now, proxyLeaderHeartbeats[socket]) > config::HEARTBEAT_TIMEOUT_SEC) {
                printf("Proxy leader failed to heartbeat to proposer %d\n", id);
                proxyLeaderHeartbeats.erase(socket);
                slowProxyLeaders.emplace_back(socket);
                iterator = fastProxyLeaders.erase(iterator);

                //send what was sent to this proxy leader to another. Won't loop forever, if at least 1 proxy leader is alive
                //will be inefficient if multiple proxy leaders fail simultaneously, and we just handed off to another failed one.
                int otherProxyLeaderSocket;
                do otherProxyLeaderSocket = fetchNextProxyLeaderSocket();
                while (otherProxyLeaderSocket == socket);
                for (const auto& [messageId, message] : proxyLeaderSentMessages[socket])
                    sendToProxyLeader(otherProxyLeaderSocket, message);
                proxyLeaderSentMessages.erase(socket);
            }
            else
                ++iterator;
        }

        //if a proxy leader has a heartbeat, move it into the fast list
        iterator = slowProxyLeaders.begin();
        while (iterator != slowProxyLeaders.end()) {
            const int socket = *iterator;
            if (difftime(now, proxyLeaderHeartbeats[socket]) > config::HEARTBEAT_TIMEOUT_SEC) {
                fastProxyLeaders.emplace_back(socket);
                iterator = slowProxyLeaders.erase(iterator);
            }
            else
                ++iterator;
        }
    }
}

[[noreturn]]
void proposer::startServer() {
    printf("Proposer Port Id: %d\n", config::PROPOSER_PORT_START + id);
    network::startServerAtPort(config::PROPOSER_PORT_START + id, [&](const int clientSocket) {
        // read first incoming message to tell who the connecting node is
        const std::optional<std::string>& incoming = network::receivePayload(clientSocket);
        if (incoming->empty())
            return;
        WhoIsThis whoIsThis;
        whoIsThis.ParseFromString(incoming.value());
        switch (whoIsThis.sender()) {
            case WhoIsThis_Sender_batcher:
                printf("Server %d connected to batcher\n", id);
                listenToBatcher(clientSocket);
            case WhoIsThis_Sender_proxyLeader:
                printf("Server %d connected to proxy leader\n", id);
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

void proposer::listenToBatcher(int socket) {
    BatcherToProposer payload;
    while (true) {
        const std::optional<std::string>& incoming = network::receivePayload(socket);
        if (incoming->empty())
            return;
        payload.ParseFromString(incoming.value());
        printf("Proposer %d received a batch request\n", id);
        std::lock_guard<std::mutex> lock(unproposedPayloadsMutex);
        unproposedPayloads.insert(unproposedPayloads.end(), payload.requests().begin(), payload.requests().end());

        payload.Clear();
    }
}

void proposer::listenToProxyLeader(int socket) {
    {std::lock_guard<std::mutex> lock(proxyLeaderMutex);
    fastProxyLeaders.emplace_back(socket);}
    proxyLeaderCV.notify_one();
    printf("Listening to proxy leader\n");
    ProxyLeaderToProposer payload;

    while (true) {
        const std::optional<std::string>& incoming = network::receivePayload(socket);
        if (incoming->empty())
            return;

        payload.ParseFromString(incoming.value());
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
                std::lock_guard<std::mutex> lock(heartbeatMutex); //TODO lock proxy leaders?
                time(&proxyLeaderHeartbeats[socket]);
                break;
            }
            default: {}
        }
        payload.Clear();
    }
}

void proposer::connectToProposers(std::map<int, std::string> proposers) {
    // Protocol is "connect to servers with a higher id than yourself, so we don't end up as both server & client for anyone
    for (const auto pair : proposers) {
        int p_id = pair.first;
        std::string ip_addr = pair.second;
        if (p_id > id) {
            const int proposerPort = config::PROPOSER_PORT_START + p_id;
            threads.emplace_back(std::thread([&, proposerPort, ip_addr]{
                const int proposerSocket = network::connectToServerAtAddress(ip_addr, proposerPort);
                network::sendPayload(proposerSocket, message::createWhoIsThis(WhoIsThis_Sender_proposer));
                printf("Proposer %d connected to other proposer\n", id);
                {std::lock_guard<std::mutex> lock(proposerMutex);
                    proposerSockets.emplace_back(proposerSocket);}
                listenToProposer(proposerSocket);
            }));
        }
    }
}

void proposer::listenToProposer(const int socket) {
    ProposerToProposer payload;
    while (true) {
        const std::optional<std::string>& incoming = network::receivePayload(socket);
        if (incoming->empty())
            return;
        payload.ParseFromString(incoming.value());

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

    std::scoped_lock lock(acceptorMutex, proxyLeaderMutex, remainingAcceptorGroupsForScoutsMutex, logMutex);
    for (const int acceptorGroupId : acceptorGroupIds) {
        remainingAcceptorGroupsForScouts.emplace(acceptorGroupId);
        const ProposerToAcceptor& p1a = message::createP1A(id, currentBallotNum, acceptorGroupId, lastCommittedSlot);
        sendToProxyLeader(fetchNextProxyLeaderSocket(), p1a);
    }
    shouldSendScouts = false;
}

void proposer::handleP1B(const ProxyLeaderToProposer& message) {
    printf("Proposer %d received p1b from acceptor group: %d, committed log length: %d, uncommitted log length: %d\n", id,
           message.acceptorgroupid(), message.committedlog_size(), message.uncommittedlog_size());

    if (message.ballot().id() != id) { // we lost the election
        // store the largest ballot we last saw so we can immediately catch up
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
    std::scoped_lock lock(acceptorGroupLogsMutex, logMutex, unproposedPayloadsMutex, ballotMutex, acceptorMutex,
                           uncommittedProposalsMutex, proxyLeaderMutex);
    Log::mergeCommittedLogs(&log, acceptorGroupCommittedLogs);
    calcLastCommittedSlot();
    const auto& [uncommittedLog, acceptorGroupForSlot] = Log::mergeUncommittedLogs(acceptorGroupUncommittedLogs);
    acceptorGroupCommittedLogs.clear();
    acceptorGroupUncommittedLogs.clear();

    for (const auto&[slot, committedPayload] : log)
        //TODO this is O(log.length * unproposedPayloads.length), not great
        unproposedPayloads.erase(std::remove(unproposedPayloads.begin(), unproposedPayloads.end(), committedPayload),unproposedPayloads.end());
    for (const auto&[slot, pValue] : uncommittedLog)
        unproposedPayloads.erase(std::remove(unproposedPayloads.begin(), unproposedPayloads.end(), pValue.payload()),unproposedPayloads.end());

    if (isLeader) {
        for (const auto& [slot, pValue] : uncommittedLog) {
            uncommittedProposals[slot] = pValue.payload();
            int acceptorGroup;
            if (acceptorGroupForSlot.find(slot) != acceptorGroupForSlot.end())
                acceptorGroup = acceptorGroupForSlot.at(slot);
            else
                acceptorGroup = fetchNextAcceptorGroupId();
            sendCommanders(acceptorGroup, slot, pValue.payload());
        }
    }
}

void proposer::sendCommandersForPayloads() {
    {std::lock_guard<std::mutex> lock(unproposedPayloadsMutex);
    if (unproposedPayloads.empty())
        return;}

    //calculate the next unused slot (log is 1-indexed, because 0 = null in protobuf & will be ignored)
    std::scoped_lock lock(uncommittedProposalsMutex, unproposedPayloadsMutex, logMutex, ballotMutex, acceptorMutex, proxyLeaderMutex);
    auto nextSlot = 1;
    for (const auto& [slot, payload] : log)
        if (slot >= nextSlot)
            nextSlot = slot + 1;
    for (const auto& [slot, proposal] : uncommittedProposals)
        if (slot >= nextSlot)
            nextSlot = slot + 1;

    for (const std::string& payload : unproposedPayloads) {
        uncommittedProposals[nextSlot] = payload;
        sendCommanders(fetchNextAcceptorGroupId(), nextSlot, payload);
        nextSlot += 1;
    }
    unproposedPayloads.clear();
}

void proposer::sendCommanders(int acceptorGroupId, int slot, const std::string& payload) {
    const ProposerToAcceptor& p2a = message::createP2A(id, ballotNum, slot, payload, acceptorGroupId);
    sendToProxyLeader(fetchNextProxyLeaderSocket(), p2a);
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

    std::scoped_lock lock(proxyLeaderMutex, acceptorGroupLogsMutex, remainingAcceptorGroupsForScoutsMutex, unproposedPayloadsMutex,
                          uncommittedProposalsMutex);
    proxyLeaderSentMessages.clear();

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

int proposer::fetchNextAcceptorGroupId() {
    nextAcceptorGroup = (nextAcceptorGroup + 1) % acceptorGroupIds.size();
    return acceptorGroupIds[nextAcceptorGroup];
}

int proposer::fetchNextProxyLeaderSocket() {
    //prioritize sending to fast proxy leaders
    if (!fastProxyLeaders.empty()) {
        nextProxyLeader = (nextProxyLeader + 1) % fastProxyLeaders.size();
        return fastProxyLeaders[nextProxyLeader];
    }
    else {
        nextProxyLeader = (nextProxyLeader + 1) % slowProxyLeaders.size();
        return nextProxyLeader;
    }
}

void proposer::sendToProxyLeader(const int proxyLeaderSocket, const ProposerToAcceptor& message) {
    proxyLeaderSentMessages[proxyLeaderSocket][message.messageid()] = message;
    network::sendPayload(proxyLeaderSocket, message);
}

void proposer::calcLastCommittedSlot() {
    while (log.find(lastCommittedSlot + 1) != log.end())
        lastCommittedSlot += 1;
}

int main(int argc, char** argv) {
    if(argc != 4) {
        printf("Please follow the format for running this function: ./proposer <PROPOSER ID> <PROPOSER FILE NAME> <ACCEPTORS FILE NAME>.\n");
        exit(0);
    }
    int proposer_id = atoi( argv[1] );
    std::string proposer_file = argv[2];
    std::map<int, std::string> proposers = parser::parse_proposer(proposer_file);
    std::string acceptor_file = argv[3];
    std::map<int, std::map<int, std::string>> acceptors = parser::parse_acceptors(acceptor_file);
    proposer(proposer_id, proposers, acceptors);
}

