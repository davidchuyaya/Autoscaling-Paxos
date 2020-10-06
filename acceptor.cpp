//
// Created by David Chu on 10/4/20.
//

#include <iostream>
#include "acceptor.hpp"
#include "utils/networkNode.hpp"

acceptor::acceptor(int id) : id(id) {
    std::cout << "acceptor is live!" << std::endl;
    startServer();
}

void acceptor::startServer() {
    network::startServerAtPort(config::ACCEPTOR_PORT_START + id, [&](int proposerSocketId) {
        listenToProposer(proposerSocketId);
    });
}

void acceptor::listenToProposer(int socket) {
    while (true) {
        std::string payload = network::receivePayload(socket);
    }
}
