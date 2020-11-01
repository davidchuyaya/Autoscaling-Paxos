//
// Created by David Chu on 10/4/20.
//

#include "acceptor.hpp"
#include "utils/network.hpp"
#include "models/message.hpp"

acceptor::acceptor(const int id, const int acceptorGroupId) : id(id), acceptorGroupId(acceptorGroupId) {
    startServer();
}

[[noreturn]]
void acceptor::startServer() {
    const int acceptorGroupPortOffset = config::ACCEPTOR_GROUP_PORT_OFFSET * acceptorGroupId;
    network::startServerAtPort(config::ACCEPTOR_PORT_START + acceptorGroupPortOffset + id, [&](const int proposerSocketId) {
        printf("Acceptor [%d, %d] connected to proxy leader\n", acceptorGroupId, id);
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
                printf("Acceptor [%d, %d] received p1a: [%d, %d], highestBallot: [%d, %d]\n", acceptorGroupId, id, payload.ballot().id(),
                       payload.ballot().ballotnum(), highestBallot.id(), highestBallot.ballotnum());
                if (Log::isBallotGreaterThan(payload.ballot(), highestBallot))
                    highestBallot = payload.ballot();
                network::sendPayload(socket, message::createP1B(payload.messageid(), acceptorGroupId, highestBallot, log));
                break;
            case ProposerToAcceptor_Type_p2a:
                printf("Acceptor [%d, %d] received p2a: [%s]\n", acceptorGroupId, id, payload.ShortDebugString().c_str());
                if (!Log::isBallotGreaterThan(highestBallot, payload.ballot())) {
                    PValue pValue;
                    pValue.set_payload(payload.payload());
                    *pValue.mutable_ballot() = payload.ballot();
                    log[payload.slot()] = pValue;
                    printf("[%d, %d] New log: %s\n", acceptorGroupId, id, Log::printLog(log).c_str());
                }
                network::sendPayload(socket, message::createP2B(payload.messageid(), acceptorGroupId, highestBallot, payload.slot()));
                break;
            default: {}
        }
        payload.Clear();
    }
}
