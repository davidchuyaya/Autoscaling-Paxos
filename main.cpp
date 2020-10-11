#include <iostream>
#include <vector>
#include <thread>
#include <numeric>
#include "main.hpp"
#include "proposer.hpp"
#include "utils/config.hpp"

int main() {
    paxos p{};
    p.start();
}

paxos::paxos() {}

[[noreturn]]
void paxos::start() {
    std::cout << "F: " << config::F << std::endl;
    setbuf(stdout, nullptr); //TODO force flush to stdout. Disable when doing metrics or in prod
    std::thread server([&]{startServer();});
    startProposers();
    startAcceptors();
    readInput();
}

void paxos::startProposers() {
    for (int i = 0; i < config::F + 1; i++) {
        participants.emplace_back(std::thread([i]{proposer {i};}));
    }
}

void paxos::startAcceptors() {
    for (int i = 0; i < 2 * config::F + 1; i++) {
        participants.emplace_back(std::thread([i]{acceptor {i};}));
    }
}

void paxos::startServer() {
    network::startServerAtPort(config::MAIN_PORT, [&](int clientSocketId) {
        std::lock_guard<std::mutex> lock(clientsMutex);
        clientSockets.emplace_back(clientSocketId);
    });
}

[[noreturn]]
void paxos::readInput() {
    while (true) {
        std::string input;
        std::cin >> input;
        broadcastToProposers(input);
    }
}

void paxos::broadcastToProposers(const std::string& payload) {
    std::lock_guard<std::mutex> lock(clientsMutex);
    for (int clientId : clientSockets) {
        network::sendPayload(clientId, payload);
    }
}
