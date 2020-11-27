//
// Created by David Chu on 10/4/20.
//

#include "acceptor.hpp"

acceptor::acceptor(const int id, std::string&& acceptorGroupId) : id(id), acceptorGroupId(acceptorGroupId) {
    std::thread server([&]{startServer();});
    server.detach();

	annaWriteOnlyClient = new anna_write_only{};
	annaWriteOnlyClient->putSingletonSet(config::KEY_ACCEPTOR_GROUPS, acceptorGroupId);
	annaWriteOnlyClient->putSingletonSet(acceptorGroupId, config::IP_ADDRESS);

	pthread_exit(nullptr);
}

[[noreturn]]
void acceptor::startServer() {
    network::startServerAtPort(config::ACCEPTOR_PORT,
       [&](const int socket, const WhoIsThis_Sender& whoIsThis) {
            LOG("Acceptor [%s, %d] connected to proxy leader\n", acceptorGroupId.c_str(), id);
        }, [&](const int socket, const WhoIsThis_Sender& whoIsThis, const std::string& payloadString) {
            ProposerToAcceptor payload;
            payload.ParseFromString(payloadString);
            listenToProxyLeaders(socket, payload);
    });
}

void acceptor::listenToProxyLeaders(const int socket, const ProposerToAcceptor& payload) {
    std::scoped_lock lock(ballotMutex, logMutex);
    switch (payload.type()) {
        case ProposerToAcceptor_Type_p1a: {
            LOG("Acceptor [%s, %d] received p1a: [%d, %d], highestBallot: [%d, %d]\n",
                   acceptorGroupId.c_str(), id, payload.ballot().id(),
                   payload.ballot().ballotnum(), highestBallot.id(), highestBallot.ballotnum());
            if (Log::isBallotGreaterThan(payload.ballot(), highestBallot))
                highestBallot = payload.ballot();
            network::sendPayload(socket, message::createP1B(payload.messageid(), acceptorGroupId, highestBallot, log));
            break;
        }
        case ProposerToAcceptor_Type_p2a:
            LOG("Acceptor [%s, %d] received p2a: [%s]\n", acceptorGroupId.c_str(), id, payload.ShortDebugString().c_str());
            if (!Log::isBallotGreaterThan(highestBallot, payload.ballot())) {
                PValue pValue;
                pValue.set_payload(payload.payload());
                *pValue.mutable_ballot() = payload.ballot();
                log[payload.slot()] = pValue;
                highestBallot = payload.ballot();
                LOG("[%s, %d] New log: %s\n", acceptorGroupId.c_str(), id, Log::printLog(log).c_str());
            }
            network::sendPayload(socket, message::createP2B(payload.messageid(), acceptorGroupId, highestBallot, payload.slot()));
            break;
        default: {}
    }
}

int main(const int argc, const char** argv) {
    if (argc != 3) {
        printf("Usage: ./acceptor <ACCEPTOR GROUP ID> <ACCEPTOR ID>.\n");
        exit(0);
    }
    const int id = std::stoi(argv[2]);
    acceptor(id, argv[1]);
}
