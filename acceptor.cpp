//
// Created by David Chu on 10/4/20.
//

#include "acceptor.hpp"

acceptor::acceptor(std::string&& acceptorGroupId) :acceptorGroupId(acceptorGroupId) {
	metricsVars = metrics::createMetricsVars({ metrics::NumProcessedMessages, metrics::P1BPreempted,
											metrics::P1BSuccess, metrics::P2BPreempted},{},{},{},
										  "acceptor" + acceptorGroupId);

	zmqNetwork = new network();

	annaClient = anna::writeOnly(zmqNetwork, {
			{config::KEY_ACCEPTOR_GROUPS, acceptorGroupId},
			{acceptorGroupId, config::IP_ADDRESS}
	});

	proxyLeaders = new server_component(zmqNetwork, config::ACCEPTOR_PORT_FOR_PROXY_LEADERS, ProxyLeader,
	                              [](const std::string& address, const time_t now) {
		BENCHMARK_LOG("Proxy leader from {} connected to acceptor", address);
	}, [&](const network::addressPayloadsMap& addressToPayloads, const time_t now) {
		listenToProxyLeaders(addressToPayloads);
	});

	zmqNetwork->poll();
}

void acceptor::listenToProxyLeaders(const network::addressPayloadsMap& addressToPayloads) {
	ProposerToAcceptor proposerToAcceptor;
	std::string reply;

	for (const auto&[address, payloads] : addressToPayloads) {
		for (const std::string& payload : payloads) {
			proposerToAcceptor.ParseFromString(payload);
			switch (proposerToAcceptor.type()) {
				case ProposerToAcceptor_Type_p1a: {
					BENCHMARK_LOG("Received p1a: {}, highestBallot: {}", proposerToAcceptor.ShortDebugString(),
					              highestBallot.ShortDebugString());
					if (Log::isBallotGreaterThan(proposerToAcceptor.ballot(), highestBallot)) {
						highestBallot = proposerToAcceptor.ballot();
						metricsVars->counters[metrics::P1BSuccess]->Increment();
					}
					else {
						metricsVars->counters[metrics::P1BPreempted]->Increment();
					}
					reply = message::createP1B(proposerToAcceptor.messageid(), acceptorGroupId, highestBallot, log)
							.SerializeAsString();
					break;
				}
				case ProposerToAcceptor_Type_p2a:
					LOG("Received p2a: {}", payload.ShortDebugString());
					TIME();
					if (!Log::isBallotGreaterThan(highestBallot, proposerToAcceptor.ballot())) {
						PValue pValue;
						pValue.set_client(proposerToAcceptor.client());
						pValue.set_payload(proposerToAcceptor.payload());
						*pValue.mutable_ballot() = proposerToAcceptor.ballot();
						log[proposerToAcceptor.slot()] = pValue;
						highestBallot = proposerToAcceptor.ballot();
						metricsVars->counters[metrics::NumProcessedMessages]->Increment();
					}
					else {
						metricsVars->counters[metrics::P2BPreempted]->Increment();
					}
					reply = message::createP2B(proposerToAcceptor.messageid(), acceptorGroupId, highestBallot,
								proposerToAcceptor.slot()).SerializeAsString();
					TIME();
					break;
				default: {}
			}
			proxyLeaders->sendToIp(address, reply);
			proposerToAcceptor.Clear();
		}
	}
}

int main(const int argc, const char** argv) {
    if (argc != 2) {
        printf("Usage: ./acceptor <ACCEPTOR GROUP ID>\n");
        exit(0);
    }

    INIT_LOGGER();
	acceptor a {argv[1]};
}
