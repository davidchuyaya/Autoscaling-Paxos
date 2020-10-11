//
// Created by David Chu on 10/4/20.
//

#include "acceptor.hpp"
#include "utils/networkNode.hpp"
#include "models/message.hpp"

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

        std::scoped_lock lock(ballotMutex, logMutex);
        switch (payload.type()) {
            case ProposerToAcceptor_Type_p1a:
                printf("Acceptor %d received p1a: [%d, %d], highestBallot: [%d, %d]\n", id, payload.ballot().id(),
                       payload.ballot().ballotnum(), highestBallot.id(), highestBallot.ballotnum());
                if (Log::isBallotGreaterThan(payload.ballot(), highestBallot))
                    highestBallot = payload.ballot();
                network::sendPayload(socket, message::createP1B(highestBallot, log).SerializeAsString());
                break;
            case ProposerToAcceptor_Type_p2a:
                printf("Acceptor %d received p2a: [%s]\n", id, payload.DebugString().c_str());
                if (!Log::isBallotGreaterThan(highestBallot, payload.ballot())) {
                    PValue pValue;
                    pValue.set_payload(payload.payload());
                    *pValue.mutable_ballot() = payload.ballot();
                    if (log.size() <= payload.slot())
                        log.resize(payload.slot() + 1);
                    log[payload.slot()] = pValue;
                    printf("New log: ");
                    Log::printLog(log);
                }
                network::sendPayload(socket, message::createP2B(highestBallot, payload.slot()).SerializeAsString());
                break;
            default: {}
        }
        payload.Clear();
    }
}
