//
// Created by David Chu on 10/4/20.
//
#include <iostream>
#include <thread>
#include "proposer.hpp"
#include "utils/config.hpp"

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
    printf("Proposer connected to main at socket: %d\n", serverSocket);

    while (true) {
        std::string payload = network::receivePayload(serverSocket);
        printf("Proposer received payload: [%s]\n", payload.c_str());
    }
}

void proposer::startServer() {
    network::startServerAtPort(config::PROPOSER_PORT_START + id, [&](int proposerSocketId) {
        listenToProposer(proposerSocketId);
    });
}

void proposer::connectToProposers() {
    //Protocol is "connect to servers with a higher id than yourself, so we don't end up as both server & client for anyone
    for (int i = id + 1; i < config::F + 1; i++) {
        int proposerPort = config::PROPOSER_PORT_START + i;
        threads.emplace_back(std::thread([&, proposerPort]{
            int proposerSocket = network::connectToServerAtAddress(config::LOCALHOST, proposerPort);
            listenToProposer(proposerSocket);
        }));
    }
}

[[noreturn]]
void proposer::listenToProposer(int socket) {
    {std::lock_guard<std::mutex> lock(proposerMutex);
        proposerSockets.emplace_back(socket);}
    while (true) {
        std::string payload = network::receivePayload(socket);
    }
}

void proposer::connectToAcceptors() {
    for (int i = 0; i < 2*config::F + 1; i++) {
        int acceptorPort = config::ACCEPTOR_PORT_START + i;
        threads.emplace_back(std::thread([&, acceptorPort]{
            int acceptorSocket = network::connectToServerAtAddress(config::LOCALHOST, acceptorPort);
            listenToAcceptor(acceptorSocket);
        }));
    }
}

void proposer::listenToAcceptor(int socket) {
    {std::lock_guard<std::mutex> lock(acceptorMutex);
        acceptorSockets.emplace_back(socket);}
    while (true) {
        std::string payload = network::receivePayload(socket);
    }
}

