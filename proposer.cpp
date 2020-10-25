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
    std::this_thread::sleep_for(std::chrono::seconds(1)); //TODO loop to see we're connected to 2F+1 acceptors
    mainLoop();
}

[[noreturn]]
void proposer::broadcastIAmLeader() {
    while (true) {
        if (isLeader) {
            time_t t;
            time(&t);
            printf("%d = leader, sending at time: %s\n", id, std::asctime(std::localtime(&t)));
            {std::lock_guard<std::mutex> lock(proposerMutex);
                network::broadcastProtobuf(message::createIamLeader(), proposerSockets);}
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
                numPreemptedScouts.clear();
                numApprovedScouts.clear();
            }
            default: {}
        }
    }
}

void proposer::connectToAcceptors() {
    for (int acceptorGroupId = 0; acceptorGroupId < config::NUM_ACCEPTOR_GROUPS; acceptorGroupId++) {
        const int acceptorGroupPortOffset = config::ACCEPTOR_GROUP_PORT_OFFSET * acceptorGroupId;
        {std::lock_guard<std::mutex> lock(acceptorMutex);
        acceptorGroupIds.emplace_back(acceptorGroupId);}

        for (int i = 0; i < 2 * config::F + 1; i++) {
            const int acceptorPort = config::ACCEPTOR_PORT_START + acceptorGroupPortOffset + i;
            threads.emplace_back(std::thread([&, acceptorPort, acceptorGroupId]{
                const int acceptorSocket = network::connectToServerAtAddress(config::LOCALHOST, acceptorPort);
                {std::lock_guard<std::mutex> lock(acceptorMutex);
                acceptorSockets[acceptorGroupId].emplace_back(acceptorSocket);}
                listenToAcceptor(acceptorSocket);
            }));
        }
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
                        numApprovedScouts[payload.acceptorgroupid()] += 1;
                }
                else
                    numPreemptedScouts[payload.acceptorgroupid()] += 1;}
                std::lock_guard<std::mutex> lock(acceptorLogsMutex);
                acceptorLogs[payload.acceptorgroupid()].emplace_back(payload.log().begin(), payload.log().end());
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

    std::lock_guard<std::mutex> acceptorLock(acceptorMutex);
    for (const auto&[acceptorGroupId, sockets] : acceptorSockets)
        network::broadcastProtobuf(p1a, sockets);
    shouldSendScouts = false;
}

void proposer::checkScouts() {
    std::unique_lock<std::mutex> lock(scoutMutex);
    for (const auto&[acceptorGroupId, approvedScoutsForAcceptorGroup] : numApprovedScouts) {
        int preemptedScoutsForAcceptorGroup = numPreemptedScouts[acceptorGroupId];
        if (approvedScoutsForAcceptorGroup + preemptedScoutsForAcceptorGroup < config::F)
            return;
        else if (preemptedScoutsForAcceptorGroup > 0) {
            isLeader = false;
            shouldSendScouts = true;
            printf("Proposer %d failed to become the leader\n", id);
            numApprovedScouts.clear();
            numPreemptedScouts.clear();
        }
    }

    //leader election complete
    isLeader = true;
    {std::lock_guard<std::mutex> proposerLock(proposerMutex);
    network::broadcastProtobuf(message::createIamLeader(), proposerSockets);}
    printf("Proposer %d is leader\n", id);
    numApprovedScouts.clear();
    numPreemptedScouts.clear();
    lock.unlock();

    mergeLogs();
}

void proposer::mergeLogs() {
    acceptorLogsMutex.lock();
    const auto& [committedLog, uncommittedLog, acceptorGroupForSlot] = Log::committedAndUncommittedLog(acceptorLogs);
    acceptorLogs.clear();
    acceptorLogsMutex.unlock();

    std::unique_lock<std::mutex> lock(unproposedPayloadsMutex);
    log = committedLog; //TODO prevent already committed item from being uncommitted?

    for (const auto&[slot, committedPayload] : log) {
        if (committedPayload.empty())
            continue;
        //TODO this is O(log.length * unproposedPayloads.length), not great
        unproposedPayloads.erase(std::remove(unproposedPayloads.begin(), unproposedPayloads.end(), committedPayload),
                                 unproposedPayloads.end());
    }
    for (const auto&[slot, pValue] : uncommittedLog) {
        if (pValue.payload().empty())
            continue;
        unproposedPayloads.erase(std::remove(unproposedPayloads.begin(), unproposedPayloads.end(), pValue.payload()),
                                 unproposedPayloads.end());
    }
    lock.unlock();

    if (isLeader) {
        std::lock_guard<std::mutex> acceptorLock(acceptorMutex);

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

    //calculate the next unused slot
    auto nextSlot = log.size();
    for (const auto& [slot, proposal] : uncommittedProposals)
        if (slot >= nextSlot)
            nextSlot = slot + 1;

    std::scoped_lock lock(unproposedPayloadsMutex, ballotMutex, acceptorMutex);
    for (const std::string& payload : unproposedPayloads) {
        uncommittedProposals[nextSlot] = payload;
        sendCommanders(fetchNextAcceptorGroup(), nextSlot, payload);
        nextSlot += 1;
    }
    unproposedPayloads.clear();
}

void proposer::sendCommanders(int acceptorGroupId, int slot, const std::string& payload) {
    const ProposerToAcceptor& p2a = message::createP2A(id, ballotNum, slot, payload);
    network::broadcastProtobuf(p2a, acceptorSockets[acceptorGroupId]);
}

int proposer::fetchNextAcceptorGroup() {
    nextAcceptorGroup = (nextAcceptorGroup + 1) % acceptorSockets.size();
    return nextAcceptorGroup;
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
        slotToApprovedCommanders.clear();
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
