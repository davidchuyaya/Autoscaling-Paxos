#include <iostream>
#include <thread>
#include <numeric>
#include "main.hpp"
#include "proposer.hpp"
#include "utils/config.hpp"
#include "proxy_leader.hpp"

int main() {
    paxos p{};
}

[[noreturn]]
paxos::paxos() {
    std::cout << "F: " << config::F << std::endl;
    setbuf(stdout, nullptr); //TODO force flush to stdout. Disable when doing metrics or in prod
    std::thread server([&]{startServer();});
    startProposers();
    startAcceptors();
    startBatchers();
    startProxyLeaders();
    readInput();
}

void paxos::startServer() {
    network::startServerAtPort(config::MAIN_PORT, [&](int clientSocketId) {
        std::lock_guard<std::mutex> lock(clientsMutex);
        clientSockets.emplace_back(clientSocketId);
    });
}

void paxos::startProposers() {
    for (int i = 0; i < config::F + 1; i++) {
        participants.emplace_back(std::thread([i]{proposer {i};}));
    }
}

void paxos::startAcceptors() {
    for (int acceptorGroupId = 0; acceptorGroupId < config::NUM_ACCEPTOR_GROUPS; acceptorGroupId++) {
        for (int i = 0; i < 2 * config::F + 1; i++) {
            participants.emplace_back(std::thread([i, acceptorGroupId]{acceptor(i, acceptorGroupId);}));
        }
    }
}

void paxos::startBatchers() {
    for (int i = 0; i < config::F + 1; i++) {
        participants.emplace_back(std::thread([i]{batcher {i};}));
    }
}

void paxos::startProxyLeaders() {
    for (int i = 0; i < config::F + 1; i++) {
        participants.emplace_back(std::thread([i]{proxy_leader {i};}));
    }
}

[[noreturn]]
void paxos::readInput() {
    while (true) {
        std::string input;
        std::cin >> input;
        sendToBatcher(input);
    }
}

// TODO retry on timeout with different batcher
void paxos::sendToBatcher(const std::string& payload) {
    std::lock_guard<std::mutex> lock(clientsMutex);
    network::sendPayload(clientSockets[batcherIndex], payload);
    batcherIndex = (batcherIndex + 1) % (clientSockets.size());
}
