//
// Created by David Chu on 1/11/21.
//

#include "mock.hpp"

mock::mock(const bool isSender, const std::string& serverAddress) : isSender(isSender), serverAddress(serverAddress) {
	metricsVars = metrics::createMetricsVars({metrics::Counter::NumSentMockMessages,
	                                          metrics::Counter::NumReceivedMockMessages}, {}, {}, {});
	printf("Server address: %s\n", serverAddress.c_str());
	zmqNetwork = new network();
}

void mock::client() {
	if (isSender)
		genericSender(Batcher, config::BATCHER_PORT_FOR_CLIENTS);
	else
		genericReceiver(Unbatcher, config::CLIENT_PORT_FOR_UNBATCHERS, false);
}

void mock::batcher() {
	if (isSender)
		customSender(Proposer, config::PROPOSER_PORT_FOR_BATCHERS, [&](){ return generateBatch(); });
	else {
		annaClient = anna::writeOnly(zmqNetwork, {{config::KEY_BATCHERS, config::IP_ADDRESS}});
		genericReceiver(Client, config::BATCHER_PORT_FOR_CLIENTS, true);
	}
}

void mock::proposerForProxyLeader(const std::vector<std::string>& acceptorGroupIds) {
	if (!isSender) {
		printf("Mock proposer cannot be receiver of proxy leader\n");
		exit(0);
	}

	annaClient = anna::writeOnly(zmqNetwork, {{config::KEY_PROPOSERS, config::IP_ADDRESS}});

	//note: can't use customSender(), since we are the server
	extraSocket = zmqNetwork->startServerAtPort(config::PROPOSER_PORT_FOR_PROXY_LEADERS,ProxyLeader);

	zmqNetwork->addHandler(ProxyLeader, [&, acceptorGroupIds]
			(const std::string& address, const std::string& payload, const time_t now) {
		int next = 0;
		while (true) { //bombard the network lol
			next = (next + 1) >= acceptorGroupIds.size() ? 0 : next + 1;
			zmqNetwork->sendToClient(extraSocket->socket, address, generateP2A(acceptorGroupIds[next]));
			incrementMetricsCounter();
		}
	});

	zmqNetwork->poll();
}

void mock::proposerForBatcher(bool isLeader) {
	if (isSender) {
		printf("Mock proposer cannot be sender of batcher\n");
		exit(0);
	}

	//note: can't use customReceiver(), since we need to tell the batcher if we are the leader
	extraSocket = zmqNetwork->startServerAtPort(config::PROPOSER_PORT_FOR_BATCHERS, Batcher);

	zmqNetwork->addHandler(Batcher, [&](const std::string& address, const std::string& payload, const time_t now) {
		if (clientAddress.empty())
			clientAddress = address;
		incrementMetricsCounter();
	});

	if (isLeader) {
		zmqNetwork->addTimer([&](const time_t now) {
			if (!clientAddress.empty()) {
				LOG("Sending I am leader");
				Ballot ballot;
				ballot.set_id(1);
				ballot.set_ballotnum(1);
				zmqNetwork->sendToClient(extraSocket->socket, clientAddress, ballot.SerializeAsString());
			}
		}, config::HEARTBEAT_SLEEP_SEC, true);
	}

	zmqNetwork->poll();
}

void mock::proxyLeaderForProposer(const std::string& acceptorGroupId) {
	if (isSender) {
		printf("Mock proxy leader cannot be sender of proposer\n");
		exit(0);
	}

	//proposer will not write unless enough acceptor groups exist
	printf("Acceptor group ID: %s\n", acceptorGroupId.c_str());
	annaClient = anna::writeOnly(zmqNetwork, {{config::KEY_ACCEPTOR_GROUPS, acceptorGroupId}});

	//note: can't use customReceiver(), since we are not the server
	extraSocket = zmqNetwork->connectToAddress(serverAddress, config::PROPOSER_PORT_FOR_PROXY_LEADERS, Proposer);
	zmqNetwork->addHandler(Proposer, [&]
			(const std::string& address, const std::string& payload, const time_t now) {
		ProposerToAcceptor proposerToAcceptor;
		proposerToAcceptor.ParseFromString(payload);

		switch (proposerToAcceptor.type()) {
			case ProposerToAcceptor_Type_p1a: {
				//proposer wins immediately, existing log is empty
				BENCHMARK_LOG("Proposer won mock phase 1");
				zmqNetwork->sendToServer(extraSocket->socket, message::createP1B(proposerToAcceptor.messageid(),
																		proposerToAcceptor.acceptorgroupid(),
																		proposerToAcceptor.ballot(), {}).SerializeAsString());
				break;
			}
			case ProposerToAcceptor_Type_p2a: {
				incrementMetricsCounter(); //do nothing
				break;
			}
			default: {};
		}
	});

	zmqNetwork->addTimer([&](const time_t now) { //heartbeat
		LOG("Sending heartbeat");
		zmqNetwork->sendToServer(extraSocket->socket, "");
	}, config::HEARTBEAT_SLEEP_SEC, true);

	zmqNetwork->poll();
}

void mock::proxyLeaderForAcceptor() {
	//can't use customSender() or customReceiver(), since we're both
	extraSocket = zmqNetwork->connectToAddress(serverAddress, config::ACCEPTOR_PORT_FOR_PROXY_LEADERS, Acceptor);
	zmqNetwork->addHandler(Acceptor, [&]
			(const std::string& address, const std::string& payload, const time_t now) {
		metricsVars->counters[metrics::Counter::NumReceivedMockMessages]->Increment();

		//resend, increase load exponentially
		for (int i = 0; i < 2; ++i) {
			zmqNetwork->sendToServer(extraSocket->socket, generateP2A());
			metricsVars->counters[metrics::Counter::NumSentMockMessages]->Increment();
		}
	});

	//send initial load
	for (int i = 0; i < 5000; ++i) {
		zmqNetwork->sendToServer(extraSocket->socket, generateP2A());
		metricsVars->counters[metrics::Counter::NumSentMockMessages]->Increment();
	}

	zmqNetwork->poll();
}

void mock::proxyLeaderForUnbatcher(const std::string& destAddress) {
	if (!isSender) {
		printf("Mock proxy leader cannot be receiver of unbatcher\n");
		exit(0);
	}

	customSender(Unbatcher, config::UNBATCHER_PORT_FOR_PROXY_LEADERS, [&, destAddress](){
		return generateBatch(destAddress);
	});
}

void mock::acceptor(const std::string& acceptorGroupId) {
	annaClient = anna::writeOnly(zmqNetwork, {
			{config::KEY_ACCEPTOR_GROUPS, acceptorGroupId},
			{acceptorGroupId, config::IP_ADDRESS}
	});

	//acceptors only talk to proxy leaders
	customReceiver(ProxyLeader, config::ACCEPTOR_PORT_FOR_PROXY_LEADERS, false, [&](const std::string& ipAddress,
			const std::string& payload, const time_t now){
		ProposerToAcceptor proposerToAcceptor;
		proposerToAcceptor.ParseFromString(payload);

		switch (proposerToAcceptor.type()) {
			case ProposerToAcceptor_Type_p1a: {
				printf("Unexpected P1A at mock acceptor\n");
				return;
			}
			case ProposerToAcceptor_Type_p2a: {
				//value immediately accepted for slot
				zmqNetwork->sendToClient(extraSocket->socket, ipAddress,
							 message::createP2B(proposerToAcceptor.messageid(), proposerToAcceptor.acceptorgroupid(),
						   proposerToAcceptor.ballot(), proposerToAcceptor.slot()).SerializeAsString());
				break;
			}
			default: {};
		}
	});
}

void mock::unbatcher() {
	if (isSender)
		genericSender(Client, config::CLIENT_PORT_FOR_UNBATCHERS);
	else {
		annaClient = anna::writeOnly(zmqNetwork, {{config::KEY_UNBATCHERS, config::IP_ADDRESS}});
		genericReceiver(ProxyLeader, config::UNBATCHER_PORT_FOR_PROXY_LEADERS, true);
	}
}

void mock::incrementMetricsCounter() {
	if (isSender)
		metricsVars->counters[metrics::Counter::NumSentMockMessages]->Increment();
	else
		metricsVars->counters[metrics::Counter::NumReceivedMockMessages]->Increment();
}

void mock::genericSender(const ComponentType type, const int port) {
	customSender(type, port, [&](){
		counter += 1;
		return std::to_string(counter);
	});
}

void mock::genericReceiver(const ComponentType type, const int port, const bool heartbeat) {
	customReceiver(type, port, heartbeat, [&](const std::string& ipAddress, const std::string& payload, const time_t now){
		incrementMetricsCounter(); //do nothing
	});
}

void mock::customSender(ComponentType type, int port, const std::function<std::string()>& generateMessage) {
	extraSocket = zmqNetwork->connectToAddress(serverAddress, port, type);
	BENCHMARK_LOG("Begin sending...");
	while (true) { //bombard the network lol
		zmqNetwork->sendToServer(extraSocket->socket, generateMessage());
		incrementMetricsCounter();
	}
}

void mock::customReceiver(ComponentType type, int port, bool heartbeat, const network::messageHandler& onReceive) {
	extraSocket = zmqNetwork->startServerAtPort(port, type);

	zmqNetwork->addHandler(type, [&](const std::string& address, const std::string& payload, const time_t now) {
		if (clientAddress.empty())
			clientAddress = address;
		onReceive(address, payload, now);
	});

	if (heartbeat) {
		zmqNetwork->addTimer([&](const time_t now) {
			if (!clientAddress.empty()) {
				LOG("Sending heartbeat");
				zmqNetwork->sendToClient(extraSocket->socket, clientAddress, "");
			}
		}, config::HEARTBEAT_SLEEP_SEC, true);
	}

	zmqNetwork->poll();
}

std::string mock::generateBatch(const std::string& ip) {
	counter += 1;
	return message::createBatchMessage(ip, std::to_string(counter)).SerializeAsString();
}

std::string mock::generateP2A(const std::string& acceptorGroupId) {
	//slot = counter, which is incrementing
	counter += 1;
	return message::createP2A(1, 1, counter, "u.nu/davidchu",std::to_string(counter),
						   acceptorGroupId).SerializeAsString();
}