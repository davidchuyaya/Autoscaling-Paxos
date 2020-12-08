//
// Created by David Chu on 10/4/20.
//

#include "acceptor.hpp"

acceptor::acceptor(std::string&& acceptorGroupId) :acceptorGroupId(acceptorGroupId) {
	annaWriteOnlyClient = anna::writeOnly({
		{config::KEY_ACCEPTOR_GROUPS, acceptorGroupId},
		{acceptorGroupId, config::IP_ADDRESS}
	});

	startServer();
}

[[noreturn]]
void acceptor::startServer() {
    network::startServerAtPort<ProposerToAcceptor>(config::ACCEPTOR_PORT,
       [&](const int socket) {
            BENCHMARK_LOG("Connected to proxy leader\n");
        }, [&](const int socket, const ProposerToAcceptor& payload) {
            listenToProxyLeaders(socket, payload);
    });
}

void acceptor::listenToProxyLeaders(const int socket, const ProposerToAcceptor& payload) {
    std::scoped_lock lock(ballotMutex, logMutex);
    switch (payload.type()) {
        case ProposerToAcceptor_Type_p1a: {
            BENCHMARK_LOG("Received p1a: {}, highestBallot: {}\n", payload.ShortDebugString(), highestBallot.ShortDebugString());
            if (Log::isBallotGreaterThan(payload.ballot(), highestBallot))
                highestBallot = payload.ballot();
            network::sendPayload(socket, message::createP1B(payload.messageid(), acceptorGroupId, highestBallot, log));
            break;
        }
        case ProposerToAcceptor_Type_p2a:
            LOG("Received p2a: {}\n", payload.ShortDebugString());
		    TIME();
            if (!Log::isBallotGreaterThan(highestBallot, payload.ballot())) {
                PValue pValue;
	            pValue.set_client(payload.client());
                pValue.set_payload(payload.payload());
                *pValue.mutable_ballot() = payload.ballot();
                log[payload.slot()] = pValue;
                highestBallot = payload.ballot();
            }
            network::sendPayload(socket, message::createP2B(payload.messageid(), acceptorGroupId, highestBallot, payload.slot()));
		    TIME();
            break;
        default: {}
    }
}

int main(const int argc, const char** argv) {
    if (argc != 2) {
        printf("Usage: ./acceptor <ACCEPTOR GROUP ID>.\n");
        exit(0);
    }

    INIT_LOGGER();
	network::ignoreClosedSocket();
	acceptor a {argv[1]};
}
