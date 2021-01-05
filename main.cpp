
#include "main.hpp"

paxos::paxos(const int delay, const int numClients, const int numBatchers, const int numProxyLeaders,
			 const int numAcceptorGroups, const int numUnbatchers) :
		shouldStartCluster(numBatchers != 0), numBatchers(numBatchers),
		numProxyLeaders(numProxyLeaders), numAcceptorGroups(numAcceptorGroups), numUnbatchers(numUnbatchers) {
	if (shouldStartCluster)
		startCluster();

	zmqNetwork = new network();

	batcherHeartbeat = new heartbeat_component(zmqNetwork);
	batchers = new client_component(zmqNetwork, config::BATCHER_PORT_FOR_CLIENTS, Batcher,
						   [&](const std::string& address, const time_t now) {
		BENCHMARK_LOG("Client connected to batcher at {}", address);
		batcherHeartbeat->addConnection(address, now);
	},
	[&](const std::string& address, const time_t now) {
		BENCHMARK_LOG("Client disconnected from batcher at {}", address);
		batcherHeartbeat->removeConnection(address);
	}, [&](const std::string& address, const std::string& payload, const time_t now) {
		LOG("Batcher {} heartbeated", address);
		batcherHeartbeat->addHeartbeat(address, now);
	});
	batchers->connectToNewMembers({{"13.57.245.102", "54.183.214.9"},{}}, 0); //TODO add new members with anna

	unbatchers = new server_component(zmqNetwork, config::CLIENT_PORT_FOR_UNBATCHERS, Unbatcher,
							 [](const std::string& address, const time_t now) {
		BENCHMARK_LOG("Unbatcher from {} connected to client", address);
	},[&](const std::string& address, const std::string& payload, const time_t now) {
		LOG("--Acked: {}--", payload);
		//send another message back immediately
		batchers->sendToIp(batcherHeartbeat->nextAddress(), payload);
	});

	//send messages to batchers after delay
	metricsVars = metrics::createMetricsVars({metrics::NumProcessedMessages},{},{},{});
	zmqNetwork->addTimer([&](const time_t t) {
		printf("Incrementing...\n");
		metricsVars->counters[metrics::NumProcessedMessages]->Increment();
	}, 1, true);

	zmqNetwork->poll();

//    annaClient = anna::readWritable({}, [&](const std::string& key, const two_p_set& twoPSet) {
//        batchers.connectAndMaybeListen(twoPSet);
//    });
//	annaClient->subscribeTo(config::KEY_BATCHERS);
}

void paxos::startCluster() {
	instanceIdsOfBatchers = scaling::startBatchers(numBatchers);
	instanceIdsOfProposers = scaling::startProposers(numAcceptorGroups);
	instanceIdsOfProxyLeaders = scaling::startProxyLeaders(numProxyLeaders);
	for (int i = 0; i < numAcceptorGroups; i++) {
		const std::string& acceptorGroupId = std::to_string(uuid::generate());
		instanceIdsOfAcceptors[acceptorGroupId] = scaling::startAcceptorGroup(acceptorGroupId);
	}
	instanceIdsOfUnbatchers = scaling::startUnbatchers(numUnbatchers);
}

int main(const int argc, const char** argv) {
    if (argc != 3 && argc != 7) {
	    printf("Usage without starting a new cluster: ./Autoscaling_Paxos <NUM CLIENTS> <SECONDS BEFORE STARTING>\n");
        printf("Usage: ./Autoscaling_Paxos <NUM CLIENTS> <SECONDS BEFORE STARTING> <NUM BATCHERS> <NUM PROXY LEADERS> <NUM ACCEPTOR GROUPS> <NUM UNBATCHERS>\n");
        exit(0);
    }

	INIT_LOGGER();

	const int numClients = std::stoi(argv[1]);
	const int delay = std::stoi(argv[2]);
	if (argc == 3) {
		paxos p {delay, numClients};
	}
	else {
		const int numBatchers = std::stoi(argv[3]);
		const int numProxyLeaders = std::stoi(argv[4]);
		const int numAcceptorGroups = std::stoi(argv[5]);
		const int numUnbatchers = std::stoi(argv[6]);
		paxos p {delay, numClients, numBatchers, numProxyLeaders, numAcceptorGroups, numUnbatchers};
	}
}