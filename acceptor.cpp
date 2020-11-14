//
// Created by David Chu on 10/4/20.
//

#include <unistd.h>
#include "acceptor.hpp"

acceptor::acceptor(const int id, const int acceptorGroupId) : id(id), acceptorGroupId(acceptorGroupId) {
    startServer();
}

[[noreturn]]
void acceptor::startServer() {
    const int acceptorGroupPortOffset = config::ACCEPTOR_GROUP_PORT_OFFSET * acceptorGroupId;
    printf("Acceptor Port: %d\n", config::ACCEPTOR_PORT_START + acceptorGroupPortOffset + id);
    network::startServerAtPort(config::ACCEPTOR_PORT_START + acceptorGroupPortOffset + id, [&](const int socket) {
        printf("Acceptor [%d, %d] connected to proxy leader\n", acceptorGroupId, id);
        listenToProxyLeaders(socket);
        close(socket);
    });
}

void acceptor::listenToProxyLeaders(const int socket) {
    ProposerToAcceptor payload;

    while (true) {
        const std::optional<std::string>& incoming = network::receivePayload(socket);
        if (incoming->empty())
            return;
        payload.ParseFromString(incoming.value());

        std::scoped_lock lock(ballotMutex, logMutex);
        switch (payload.type()) {
            case ProposerToAcceptor_Type_p1a: {
                printf("Acceptor [%d, %d] received p1a: [%d, %d], slot: %d, highestBallot: [%d, %d]\n",
                       acceptorGroupId, id, payload.ballot().id(),
                       payload.ballot().ballotnum(), payload.slot(), highestBallot.id(), highestBallot.ballotnum());
                if (Log::isBallotGreaterThan(payload.ballot(), highestBallot))
                    highestBallot = payload.ballot();
                const Log::pValueLog& filteredLog = logAfterSlot(payload.slot());
                network::sendPayload(socket, message::createP1B(payload.messageid(), acceptorGroupId, highestBallot,
                                                                filteredLog));
                break;
            }
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

Log::pValueLog acceptor::logAfterSlot(const int slotToFilter) {
    Log::pValueLog filteredLog = {};
    for (const auto&[slot, pValue] : log)
        if (slot > slotToFilter)
            filteredLog[slot] = pValue;
    return filteredLog;
}

int main(const int argc, const char** argv) {
    if (argc != 3) {
        printf("Usage: ./acceptor <ACCEPTOR GROUP ID> <ACCEPTOR ID>.\n");
        exit(0);
    }
    const int acceptorGroupId = atoi(argv[1] );
    const int id = atoi(argv[2] );
    acceptor(id, acceptorGroupId);
}
