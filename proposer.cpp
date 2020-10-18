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
    const std::thread server([&] {startServer(); });
    connectToProposers();
    connectToAcceptors();
    const std::thread broadcastLeader([&] {broadcastIAmLeader(); });
    std::this_thread::sleep_for(std::chrono::seconds(1)); //TODO loop to see we're connected to F+1 acceptors
    mainLoop();
}

[[noreturn]]
void proposer::broadcastIAmLeader() {
    while (true) {
        if (isLeader) {
            time_t t;
            time(&t);
            printf("%d = leader, sending at time: %s\n", id, std::asctime(std::localtime(&t)));
            broadcastToProposers(message::createIamLeader());
        }
        std::this_thread::sleep_for(std::chrono::seconds(config::LEADER_HEARTBEAT_SLEEP_SEC));
    }
}

[[noreturn]]
void proposer::startServer() {
    network::startServerAtPort(config::PROPOSER_PORT_START + id, [&](const int proposerSocketId) {
        {std::lock_guard<std::mutex> lock(proposerMutex);
            proposerSockets.emplace_back(proposerSocketId);}
        listenToProposer(proposerSocketId);
    });
}

void proposer::connectToProposers() {
    //Protocol is "connect to servers with a higher id than yourself, so we don't end up as both server & client for anyone
    for (int i = id + 1; i < config::F + 1; i++) {
        const int proposerPort = config::PROPOSER_PORT_START + i;
        threads.emplace_back(std::thread([&, proposerPort]{
            const int proposerSocket = network::connectToServerAtAddress(config::LOCALHOST, proposerPort);
            printf("Proposer %d connected to other proposer\n", id);
            {std::lock_guard<std::mutex> lock(proposerMutex);
                proposerSockets.emplace_back(proposerSocket);}
            listenToProposer(proposerSocket);
        }));
    }
}

[[noreturn]]
void proposer::listenToProposer(const int socket) {
    ProposerReceiver payload;
    while (true) {
        payload.ParseFromString(network::receivePayload(socket));
        switch (payload.sender()){
            case ProposerReceiver_Sender_batcher: {
                printf("Proposer %d received a batch request\n", id);
                {
                    std::lock_guard<std::mutex> lock(unproposedPayloadsMutex);
                    for (const auto& request: payload.requests()) {
                        unproposedPayloads.push_back(request);
                    }
                }
            }
            case ProposerReceiver_Sender_proposer: {
                std::scoped_lock lock(leaderHeartbeatMutex, scoutMutex);
                printf("%d received leader heartbeat for time: %s\n", id,
                       std::asctime(std::localtime(&lastLeaderHeartbeat)));
                time(&lastLeaderHeartbeat); // store the time we received the heartbeat
                isLeader = false;
                shouldSendScouts = true;
                numPreemptedScouts = 0;
                numApprovedScouts = 0;
            }
            default: {}
        }
    }
}

void proposer::broadcastToProposers(const google::protobuf::Message& message) {
    const std::string& serializedMessage = message.SerializeAsString();

    std::lock_guard<std::mutex> lock(proposerMutex);
    for (const int socket : proposerSockets) {
        network::sendPayload(socket, serializedMessage);
    }
}

void proposer::connectToAcceptors() {
    for (int i = 0; i < 2*config::F + 1; i++) {
        const int acceptorPort = config::ACCEPTOR_PORT_START + i;
        threads.emplace_back(std::thread([&, acceptorPort]{
            const int acceptorSocket = network::connectToServerAtAddress(config::LOCALHOST, acceptorPort);
            {std::lock_guard<std::mutex> lock(acceptorMutex);
                acceptorSockets.emplace_back(acceptorSocket);}
            listenToAcceptor(acceptorSocket);
        }));
    }
}

void proposer::listenToAcceptor(const int socket) {
    AcceptorToProposer payload;
    while (true) {
        payload.ParseFromString(network::receivePayload(socket));

        switch (payload.type()) {
            case AcceptorToProposer_Type_p1b: {
                printf("Proposer %d received p1b, highest ballot: [%d, %d], log length: %d\n", id,  payload.ballot().id(),
                       payload.ballot().ballotnum(), payload.log_size());
                {std::scoped_lock lock(scoutMutex, ballotMutex);
                if (payload.ballot().id() == id) {
                    if (payload.ballot().ballotnum() == ballotNum)
                        numApprovedScouts += 1;
                }
                else
                    numPreemptedScouts += 1;}
                std::lock_guard<std::mutex> lock(acceptorLogsMutex);
                acceptorLogs.emplace_back(payload.log().begin(), payload.log().end());
                break;
            }
            case AcceptorToProposer_Type_p2b: {
                printf("Proposer %d received p2b, highest ballot: [%d, %d]\n", id,  payload.ballot().id(),
                    payload.ballot().ballotnum());
                std::scoped_lock lock(commanderMutex, ballotMutex);
                if (payload.ballot().id() == id) {
                    if (payload.ballot().ballotnum() == ballotNum)
                        slotToApprovedCommanders[payload.slot()] += 1;
                }
                else
                    slotToPreemptedCommanders[payload.slot()] += 1;
                break;
            }
            default: {}
        }
        payload.Clear();
    }
}

void proposer::broadcastToAcceptors(const google::protobuf::Message& message) {
    const std::string& serializedMessage = message.SerializeAsString();

    std::lock_guard<std::mutex> lock(acceptorMutex);
    for (const int socket : acceptorSockets) {
        network::sendPayload(socket, serializedMessage);
    }
}

[[noreturn]]
void proposer::mainLoop() {
    time_t now;

    while (true) {
        //go back to sleep if leader heartbeat detected
        {time(&now);
        std::unique_lock lock(leaderHeartbeatMutex);
        if (difftime(now, lastLeaderHeartbeat) < config::LEADER_TIMEOUT_SEC) {
            lock.unlock();
            std::this_thread::sleep_for(std::chrono::seconds(config::LEADER_TIMEOUT_SEC));
            continue;
        }}

        if (shouldSendScouts)
            sendScouts();
        if (isLeader) {
            sendCommandersForPayloads();
            checkCommanders();
        }
        else
            checkScouts();
    }
}

void proposer::sendScouts() {
    //TODO set random timeout so a leader is easily elected
    int currentBallotNum;
    {std::lock_guard<std::mutex> lock(ballotMutex);
        ballotNum += 1;
        currentBallotNum = ballotNum;}
    const ProposerToAcceptor& p1a = message::createP1A(id, currentBallotNum);
    printf("P1A blasting out: id = %d, ballotNum = %d\n", id, currentBallotNum);
    broadcastToAcceptors(p1a);
    shouldSendScouts = false;
}

void proposer::checkScouts() {
    std::unique_lock<std::mutex> lock(scoutMutex);
    if (numApprovedScouts + numPreemptedScouts <= config::F)
        return;

    //leader election complete
    if (numApprovedScouts > config::F) {
        isLeader = true;
        broadcastToProposers(message::createIamLeader());
        printf("Proposer %d is leader\n", id);
    }
    else {
        isLeader = false;
        shouldSendScouts = true;
        printf("Proposer %d failed to become the leader\n", id);
    }
    numApprovedScouts = 0;
    numPreemptedScouts = 0;
    lock.unlock();

    mergeLogs();
}

void proposer::mergeLogs() {
    acceptorLogsMutex.lock();
    const auto& [committedLog, uncommittedLog] = Log::committedAndUncommittedLog(acceptorLogs);
    acceptorLogs.clear();
    acceptorLogsMutex.unlock();

    std::lock_guard<std::mutex> lock(unproposedPayloadsMutex);
    log = committedLog; //TODO prevent already committed item from being uncommitted?

    for (const std::string& committedPayload : log) {
        if (committedPayload.empty())
            continue;
        //TODO this is O(log.length * unproposedPayloads.length), not great
        unproposedPayloads.erase(std::remove(unproposedPayloads.begin(), unproposedPayloads.end(), committedPayload),
                                 unproposedPayloads.end());
    }
    for (const auto& [slot, payload] : uncommittedLog) {
        if (payload.empty())
            continue;
        unproposedPayloads.erase(std::remove(unproposedPayloads.begin(), unproposedPayloads.end(), payload),
                                 unproposedPayloads.end());
        if (isLeader) {
            uncommittedProposals[slot] = payload;
            sendCommanders(slot, payload);
        }
    }
}

void proposer::sendCommandersForPayloads() {
    std::lock_guard<std::mutex> lock(unproposedPayloadsMutex);
    if (unproposedPayloads.empty())
        return;

    //calculate the next unused slot
    auto nextSlot = log.size();
    for (const auto& [slot, proposal] : uncommittedProposals)
        if (slot >= nextSlot)
            nextSlot = slot + 1;

    for (const std::string& payload : unproposedPayloads) {
        uncommittedProposals[nextSlot] = payload;
        sendCommanders(nextSlot, payload);
        nextSlot += 1;
    }
    unproposedPayloads.clear();
}

void proposer::sendCommanders(const int slot, const std::string &payload) {
    std::lock_guard<std::mutex> lock(ballotMutex);
    const ProposerToAcceptor& p2a = message::createP2A(id, ballotNum, slot, payload);
    broadcastToAcceptors(p2a);
}

void proposer::checkCommanders() {
    std::scoped_lock lock(commanderMutex, unproposedPayloadsMutex);
    // loop over iterator since we need to remove committed proposals as we go
    for (auto iterator = uncommittedProposals.begin(); iterator != uncommittedProposals.end();) {
        const int slot = iterator->first;
        const std::string& payload = iterator->second;
        const int numApproved = slotToApprovedCommanders[slot];
        const int numPreempted = slotToPreemptedCommanders[slot];

        if (numApproved + numPreempted <= config::F) {
            iterator++;
            continue;
        }
        if (numApproved > config::F) {
            // proposal is committed
            if (log.size() <= slot)
                log.resize(slot + 1);
            log[slot] = payload;
            // remove from uncommitted proposals
            iterator = uncommittedProposals.erase(iterator);
        }
        // we've been preempted by a new leader
        else {
            shouldSendScouts = true;
            isLeader = false;
            iterator++;
        }

        slotToApprovedCommanders.erase(slot);
        slotToPreemptedCommanders.erase(slot);
    }

    if (!isLeader) {
        slotToPreemptedCommanders.clear();
        slotToPreemptedCommanders.clear();

        std::vector<std::string> unslottedProposals = {};
        unslottedProposals.reserve(uncommittedProposals.size());
        for (const auto& [slot, payload] : uncommittedProposals) {
            unslottedProposals.emplace_back(payload);
        }
        unproposedPayloads.insert(unproposedPayloads.begin(), unslottedProposals.begin(), unslottedProposals.end());
        uncommittedProposals.clear();
    }
}
