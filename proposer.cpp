//
// Created by David Chu on 10/4/20.
//
#include <iostream>
#include <thread>
#include "proposer.hpp"
#include "utils/config.hpp"
#include "messaging/message.hpp"

proposer::proposer(const int id) : id(id) {
    printf("Proposer is live!\n");
    std::thread server([&] {startServer(); });
    connectToProposers();
    connectToAcceptors();
    listenToMain();
}

[[noreturn]]
void proposer::listenToMain() {
    int serverSocket = network::connectToServerAtAddress(config::LOCALHOST, config::MAIN_PORT);
    printf("Proposer %d connected to main\n", id);

    while (true) {
        std::string payload = network::receivePayload(serverSocket);
        printf("Proposer %d received payload: [%s]\n", id, payload.c_str());

        ProposerToAcceptor p1a = message::createP1A(id, ballotNum);
        broadcastToAcceptors(p1a);
    }
}

void proposer::startServer() {
    network::startServerAtPort(config::PROPOSER_PORT_START + id, [&](int proposerSocketId) {
        storeProposerSocket(proposerSocketId);
        listenToProposer(proposerSocketId);
    });
}

void proposer::connectToProposers() {
    //Protocol is "connect to servers with a higher id than yourself, so we don't end up as both server & client for anyone
    for (int i = id + 1; i < config::F + 1; i++) {
        int proposerPort = config::PROPOSER_PORT_START + i;
        threads.emplace_back(std::thread([&, proposerPort]{
            int proposerSocket = network::connectToServerAtAddress(config::LOCALHOST, proposerPort);
            printf("Proposer %d connected to other proposer", id);
            storeProposerSocket(proposerSocket);
            listenToProposer(proposerSocket);
        }));
    }
}

void proposer::storeProposerSocket(int socket) {
    std::lock_guard<std::mutex> lock(proposerMutex);
    proposerSockets.emplace_back(socket);
}

[[noreturn]]
void proposer::listenToProposer(int socket) {
    while (true) {
        std::string payload = network::receivePayload(socket);
    }
}

void proposer::connectToAcceptors() {
    for (int i = 0; i < 2*config::F + 1; i++) {
        int acceptorPort = config::ACCEPTOR_PORT_START + i;
        threads.emplace_back(std::thread([&, acceptorPort]{
            int acceptorSocket = network::connectToServerAtAddress(config::LOCALHOST, acceptorPort);
            storeAcceptorSocket(acceptorSocket);
            listenToAcceptor(acceptorSocket);
        }));
    }
}

void proposer::listenToAcceptor(int socket) {
    while (true) {
        std::string payload = network::receivePayload(socket);
    }
}

void proposer::storeAcceptorSocket(int socket) {
    std::lock_guard<std::mutex> lock(acceptorMutex);
    acceptorSockets.emplace_back(socket);
}

void proposer::broadcastToAcceptors(const google::protobuf::Message& message) {
    std::string serializedMessage = message.SerializeAsString();

    std::lock_guard<std::mutex> lock(acceptorMutex);
    for (int socket : acceptorSockets) {
        network::sendPayload(socket, serializedMessage);
    }
}

