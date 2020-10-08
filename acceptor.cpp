//
// Created by David Chu on 10/4/20.
//

#include "acceptor.hpp"
#include "utils/networkNode.hpp"

acceptor::acceptor(int id) : id(id) {
    startServer();
}

void acceptor::startServer() {
    network::startServerAtPort(config::ACCEPTOR_PORT_START + id, [&](int proposerSocketId) {
        printf("Acceptor %d connected to proposer\n", id);
        listenToProposer(proposerSocketId);
    });
}

void acceptor::listenToProposer(int socket) {
    ProposerToAcceptor payload;

    while (true) {
        payload.ParseFromString(network::receivePayload(socket));
        printf("Acceptor %d received payload: [%s]\n", id, payload.DebugString().c_str());
        switch (payload.type()) {
            case ProposerToAcceptor_Type_p1a:
                break;
            case ProposerToAcceptor_Type_p2a:
                break;
        }
        payload.Clear();
    }
}
