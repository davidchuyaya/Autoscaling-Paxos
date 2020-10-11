//
// Created by David Chu on 10/4/20.
//
#include <thread>
#include <algorithm>
#include "proposer.hpp"
#include "utils/config.hpp"
#include "models/message.hpp"

proposer::proposer(const int id) : id(id) {
    const std::thread server([&] {startServer(); });
    connectToProposers();
    connectToAcceptors();
    const std::thread connectionToMain([&] {listenToMain(); });
    std::this_thread::sleep_for(std::chrono::seconds(1)); //TODO loop to see we're connected to F+1 acceptors
    mainLoop();
}

[[noreturn]]
void proposer::listenToMain() {
    const int serverSocket = network::connectToServerAtAddress(config::LOCALHOST, config::MAIN_PORT);
    printf("Proposer %d connected to main\n", id);

    while (true) {
        std::string payload = network::receivePayload(serverSocket);
        printf("Proposer %d received payload: [%s]\n", id, payload.c_str());

        std::lock_guard<std::mutex> lock(unproposedPayloadsMutex);
        unproposedPayloads.emplace_back(payload);
    }
}

void proposer::startServer() {
    network::startServerAtPort(config::PROPOSER_PORT_START + id, [&](const int proposerSocketId) {
        storeProposerSocket(proposerSocketId);
        listenToProposer(proposerSocketId);
    });
}

void proposer::connectToProposers() {
    //Protocol is "connect to servers with a higher id than yourself, so we don't end up as both server & client for anyone
    for (int i = id + 1; i < config::F + 1; i++) {
        const int proposerPort = config::PROPOSER_PORT_START + i;
        threads.emplace_back(std::thread([&, proposerPort]{
            const int proposerSocket = network::connectToServerAtAddress(config::LOCALHOST, proposerPort);
            printf("Proposer %d connected to other proposer", id);
            storeProposerSocket(proposerSocket);
            listenToProposer(proposerSocket);
        }));
    }
}

void proposer::storeProposerSocket(const int socket) {
    std::lock_guard<std::mutex> lock(proposerMutex);
    proposerSockets.emplace_back(socket);
}

[[noreturn]]
void proposer::listenToProposer(const int socket) {
    while (true) {
        std::string payload = network::receivePayload(socket);
        //TODO stable leader
    }
}

void proposer::connectToAcceptors() {
    for (int i = 0; i < 2*config::F + 1; i++) {
        const int acceptorPort = config::ACCEPTOR_PORT_START + i;
        threads.emplace_back(std::thread([&, acceptorPort]{
            const int acceptorSocket = network::connectToServerAtAddress(config::LOCALHOST, acceptorPort);
            storeAcceptorSocket(acceptorSocket);
            listenToAcceptor(acceptorSocket);
        }));
    }
}

void proposer::storeAcceptorSocket(const int socket) {
    std::lock_guard<std::mutex> lock(acceptorMutex);
    acceptorSockets.emplace_back(socket);
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
    while (true) {
        if (shouldSendScouts)
            sendScouts();
        if (isLeader) {
            sendCommandersForPayloads();
            checkCommanders();
        }
        else
            checkScouts();
        std::this_thread::sleep_for(std::chrono::seconds(1));
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
    std::scoped_lock lock(scoutMutex, commanderMutex, acceptorLogsMutex, unproposedPayloadsMutex); //TODO fix locking clusterf*ck?
    if (numApprovedScouts + numPreemptedScouts <= config::F)
        return;

    //leader election complete
    if (numApprovedScouts > config::F) {
        isLeader = true;
        printf("Proposer %d is leader, unproposed payloads size: %lu\n", id, unproposedPayloads.size());
    }
    else {
        isLeader = false;
        shouldSendScouts = true;
        printf("Proposer %d failed to become the leader\n", id);
    }
    numApprovedScouts = 0;
    numPreemptedScouts = 0;

    const auto& [committedLog, uncommittedLog] = Log::committedAndUncommittedLog(acceptorLogs);
    acceptorLogs.clear();

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
    std::vector<int> slotsToRemove = {};
    for (const auto& [slot, payload] : uncommittedProposals) {
        const int numApproved = slotToApprovedCommanders[slot];
        const int numPreempted = slotToPreemptedCommanders[slot];

        if (numApproved + numPreempted <= config::F)
            continue;
        if (numApproved > config::F) {
            // proposal is committed
            if (log.size() <= slot)
                log.resize(slot + 1);
            log[slot] = payload;
            slotsToRemove.emplace_back(slot);
        }
        // we've been preempted by a new leader
        else {
            shouldSendScouts = true;
            isLeader = false;
        }

        slotToApprovedCommanders.erase(slot);
        slotToPreemptedCommanders.erase(slot);
    }

    //TODO clear uncommittedProposals

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
