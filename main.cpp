#include <iostream>
#include <vector>
#include <thread>
#include "main.hpp"
#include "proposer.hpp"

int main() {
    paxos p {1};
    p.start();
}

paxos::paxos(int f) : f(f) {}

[[noreturn]]
void paxos::start() {
    std::cout << "F: " << f << std::endl;
    startProposers();
    startAcceptors();
    network::startServerAtPort(10000, [](int clientSocketId) {

    });
}



void paxos::startProposers() {
    for (int i = 0; i < f + 1; i++) {
        participants.emplace_back(std::thread([]{proposer();}));
    }
}

void paxos::startAcceptors() {
    std::vector<acceptor> acceptors (2*f+1);
}