//
// Created by David Chu on 10/4/20.
//

#include "acceptor.hpp"

acceptor::acceptor(std::string&& acceptorGroupId) :acceptorGroupId(acceptorGroupId) {
	metricsVars = metrics::createMetricsVars({ metrics::NumProcessedMessages, metrics::P1BPreempted,
											metrics::P1BSuccess, metrics::P2BPreempted},{},{},{});

	zmqNetwork = new network();

	annaClient = anna::writeOnly(zmqNetwork, {
			{config::KEY_ACCEPTOR_GROUPS, acceptorGroupId},
			{acceptorGroupId, config::IP_ADDRESS}
	});

	proxyLeaders = new server_component(zmqNetwork, config::ACCEPTOR_PORT_FOR_PROXY_LEADERS, ProxyLeader,
	                              [](const std::string& address, const time_t now) {
		BENCHMARK_LOG("Proxy leader from {} connected to acceptor", address);
	}, [&](const std::string& address, const std::string& payload, const time_t now) {
		ProposerToAcceptor proposerToAcceptor;
		proposerToAcceptor.ParseFromString(payload);
		listenToProxyLeaders(address, proposerToAcceptor);
	});

	zmqNetwork->poll();
}

void acceptor::listenToProxyLeaders(const std::string& ipAddress, const ProposerToAcceptor& payload) {
	std::string reply;
    switch (payload.type()) {
        case ProposerToAcceptor_Type_p1a: {
            BENCHMARK_LOG("Received p1a: {}, highestBallot: {}", payload.ShortDebugString(),
						  highestBallot.ShortDebugString());
            if (Log::isBallotGreaterThan(payload.ballot(), highestBallot)) {
	            highestBallot = payload.ballot();
	            metricsVars->counters[metrics::P1BSuccess]->Increment();
            }
            else {
	            metricsVars->counters[metrics::P1BPreempted]->Increment();
            }
	        reply = message::createP1B(payload.messageid(), acceptorGroupId, highestBallot, log).SerializeAsString();
            break;
        }
        case ProposerToAcceptor_Type_p2a:
            LOG("Received p2a: {}", payload.ShortDebugString());
		    TIME();
            if (!Log::isBallotGreaterThan(highestBallot, payload.ballot())) {
                PValue pValue;
	            pValue.set_client(payload.client());
                pValue.set_payload(payload.payload());
                *pValue.mutable_ballot() = payload.ballot();
                log[payload.slot()] = pValue;
                highestBallot = payload.ballot();
	            metricsVars->counters[metrics::NumProcessedMessages]->Increment();
            }
            else {
	            metricsVars->counters[metrics::P2BPreempted]->Increment();
            }
            reply = message::createP2B(payload.messageid(), acceptorGroupId, highestBallot, payload.slot())
            		.SerializeAsString();
		    TIME();
            break;
        default: {}
    }
	proxyLeaders->sendToIp(ipAddress, reply);
}

int main(const int argc, const char** argv) {
    if (argc != 2) {
        printf("Usage: ./acceptor <ACCEPTOR GROUP ID>\n");
        exit(0);
    }

    INIT_LOGGER();
	acceptor a {argv[1]};
}
