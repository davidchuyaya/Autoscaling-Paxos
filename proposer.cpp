//
// Created by David Chu on 10/4/20.
//
#include <thread>
#include <algorithm>
#include <google/protobuf/message.h>
#include "proposer.hpp"
#include "utils/config.hpp"
#include "models/message.hpp"

proposer::proposer(const int id, const parser::idToIP& proposers, const std::unordered_map<int, parser::idToIP>& acceptors) : id(id), proxyLeaders(config::F+1) {
    findAcceptorGroupIds(acceptors);
    std::thread server([&] {startServer(); });
    server.detach();
    connectToProposers(proposers);
    std::thread broadcastLeader([&] { broadcastIAmLeader(); });
    broadcastLeader.detach();
    std::thread heartbeatChecker([&] { checkHeartbeats(); });
    heartbeatChecker.detach();
    printf("Waiting for proxy leaders\n");
    proxyLeaders.waitForThreshold();
    printf("Starting main loop\n");
    mainLoop();
    pthread_exit(nullptr);
}

void proposer::findAcceptorGroupIds(const std::unordered_map<int, parser::idToIP>& acceptors) {
    std::unique_lock lock(acceptorMutex);
    for (const auto& [acceptorGroupId, acceptorGroupMembers] : acceptors)
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
            std::shared_lock lock(proposerMutex);
            for (const int proposerSocket: proposerSockets)
                network::sendPayload(proposerSocket, iAmLeader);
        }
        std::this_thread::sleep_for(std::chrono::seconds(config::HEARTBEAT_SLEEP_SEC));
    }
}

[[noreturn]]
void proposer::checkHeartbeats() {
    time_t now;
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(config::HEARTBEAT_TIMEOUT_SEC));
        std::shared_lock lock(heartbeatMutex);
        time(&now);
        if (difftime(now, lastLeaderHeartbeat) > config::HEARTBEAT_TIMEOUT_SEC && !isLeader)
            shouldSendScouts = true;
    }
}

[[noreturn]]
void proposer::startServer() {
    printf("Proposer Port Id: %d\n", config::PROPOSER_PORT_START + id);
    network::startServerAtPort(config::PROPOSER_PORT_START + id,
           [&](const int socket, const WhoIsThis_Sender& whoIsThis) {
            switch (whoIsThis) {
                case WhoIsThis_Sender_batcher:
                    printf("Server %d connected to batcher\n", id);
                    break;
                case WhoIsThis_Sender_proxyLeader:
                    printf("Server %d connected to proxy leader\n", id);
                    proxyLeaders.addConnection(socket);
                    break;
                case WhoIsThis_Sender_proposer: {
                    printf("Server %d connected to proposer\n", id);
                    std::unique_lock lock(proposerMutex);
                    proposerSockets.emplace_back(socket);
                    break;
                }
                default: {}
            }
        }, [&](const int socket, const WhoIsThis_Sender& whoIsThis, const std::string& payloadString) {
            switch (whoIsThis) {
                case WhoIsThis_Sender_batcher:
                    listenToBatcher(payloadString);
                    break;
                case WhoIsThis_Sender_proxyLeader: {
                    ProxyLeaderToProposer payload;
                    payload.ParseFromString(payloadString);
                    listenToProxyLeader(socket, payload);
                    break;
                }
                case WhoIsThis_Sender_proposer:
                    listenToProposer();
                    break;
                default: {}
            }
    });
}

void proposer::listenToBatcher(const std::string& payload) {
    printf("Proposer %d received a batch request\n", id);
    std::unique_lock lock(unproposedPayloadsMutex);
    unproposedPayloads.emplace_back(payload);
}

void proposer::listenToProxyLeader(const int socket, const ProxyLeaderToProposer& payload) {
    switch (payload.type()) {
        case ProxyLeaderToProposer_Type_p1b:
            handleP1B(payload);
            break;
        case ProxyLeaderToProposer_Type_p2b:
            handleP2B(payload);
            break;
        case ProxyLeaderToProposer_Type_heartbeat: {
            proxyLeaders.addHeartbeat(socket);
            break;
        }
        default: {}
    }
}

void proposer::connectToProposers(const parser::idToIP& proposers) {
    for (const auto& idToIP : proposers) {
        int proposerID = idToIP.first;
        std::string proposerIP = idToIP.second;

        //Connect to servers with a higher id than yourself, so we don't end up as both server & client for anyone
        if (proposerID <= id)
            continue;

        const int proposerPort = config::PROPOSER_PORT_START + proposerID;
        std::thread thread([&, proposerPort, proposerIP]{
            const int socket = network::connectToServerAtAddress(proposerIP, proposerPort, WhoIsThis_Sender_proposer);
            printf("Proposer %d connected to other proposer\n", id);
            std::unique_lock lock(proposerMutex);
            proposerSockets.emplace_back(socket);
            lock.unlock();
            network::listenToSocketUntilClose(socket, [&](const int socket, const std::string& payload) {
                listenToProposer();
            });
        });
        thread.detach();
    }
}

void proposer::listenToProposer() {
    std::unique_lock lock(heartbeatMutex);
    printf("%d received leader heartbeat for time: %s\n", id,
           std::asctime(std::localtime(&lastLeaderHeartbeat)));
    time(&lastLeaderHeartbeat); // store the time we received the heartbeat

    noLongerLeader();
    shouldSendScouts = false; // disable scouts until the leader's heartbeat times out
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
    std::unique_lock ballotLock(ballotMutex);
    ballotNum += 1;
    currentBallotNum = ballotNum;
    ballotLock.unlock();
    printf("P1A blasting out: id = %d, ballotNum = %d\n", id, currentBallotNum);

    std::shared_lock acceptorsLock(acceptorMutex, std::defer_lock);
    std::shared_lock logLock(logMutex, std::defer_lock);
    std::scoped_lock lock(remainingAcceptorGroupsForScoutsMutex, logLock, acceptorsLock);
    for (const int acceptorGroupId : acceptorGroupIds) {
        remainingAcceptorGroupsForScouts.emplace(acceptorGroupId);
        proxyLeaders.send(message::createP1A(id, currentBallotNum, acceptorGroupId, lastCommittedSlot));
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
    std::shared_lock proposerLock(proposerMutex);
    for (const int proposerSocket: proposerSockets)
        network::sendPayload(proposerSocket, iAmLeader);
    proposerLock.unlock();

    mergeLogs();
}

void proposer::mergeLogs() {
    std::shared_lock ballotLock(ballotMutex, std::defer_lock);
    std::shared_lock acceptorLock(acceptorMutex, std::defer_lock);
    std::scoped_lock lock(acceptorGroupLogsMutex, logMutex, unproposedPayloadsMutex, ballotLock, acceptorLock,
                           uncommittedProposalsMutex);
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
            proxyLeaders.send(message::createP2A(id, ballotNum, slot, pValue.payload(), acceptorGroup));
        }
    }
}

void proposer::sendCommandersForPayloads() {
    std::shared_lock unproposedPayloadsLock(unproposedPayloadsMutex);
    if (unproposedPayloads.empty())
        return;
    unproposedPayloadsLock.unlock();

    //calculate the next unused slot (log is 1-indexed, because 0 = null in protobuf & will be ignored)
    std::shared_lock ballotLock(ballotMutex, std::defer_lock);
    std::shared_lock acceptorLock(acceptorMutex, std::defer_lock);
    std::shared_lock logLock(logMutex, std::defer_lock);
    std::scoped_lock lock(uncommittedProposalsMutex, unproposedPayloadsMutex, logLock, ballotLock, acceptorLock);
    //TODO more efficient next slot calculations
    auto nextSlot = 1;
    for (const auto& [slot, payload] : log)
        if (slot >= nextSlot)
            nextSlot = slot + 1;
    for (const auto& [slot, proposal] : uncommittedProposals)
        if (slot >= nextSlot)
            nextSlot = slot + 1;

    for (const std::string& payload : unproposedPayloads) {
        uncommittedProposals[nextSlot] = payload;
        proxyLeaders.send(message::createP2A(id, ballotNum, nextSlot, payload,
                                             fetchNextAcceptorGroupId()));
        nextSlot += 1;
    }
    unproposedPayloads.clear();
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

    std::scoped_lock lock(acceptorGroupLogsMutex, remainingAcceptorGroupsForScoutsMutex, unproposedPayloadsMutex,
                          uncommittedProposalsMutex);

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

void proposer::calcLastCommittedSlot() {
    while (log.find(lastCommittedSlot + 1) != log.end())
        lastCommittedSlot += 1;
}

int main(const int argc, const char** argv) {
    if (argc != 4) {
        printf("Usage: ./proposer <PROPOSER ID> <PROPOSER FILE NAME> <ACCEPTORS FILE NAME>.\n");
        exit(0);
    }
    const int id = atoi( argv[1] );
    const std::string& proposerFileName = argv[2];
    const parser::idToIP& proposers = parser::parseIDtoIPs(proposerFileName);
    const std::string& acceptorFileName = argv[3];
    const std::unordered_map<int, parser::idToIP>& acceptors = parser::parseAcceptors(acceptorFileName);
    proposer(id, proposers, acceptors);
}

