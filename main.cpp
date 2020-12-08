
#include "main.hpp"

paxos::paxos(const int numClients, const int numBatchers, const int numProxyLeaders, const int numAcceptorGroups,
			 const int numUnbatchers) : isBenchmark(numClients != 0), numClients(numClients),
			 numBatchers(numBatchers), numProxyLeaders(numProxyLeaders), numAcceptorGroups(numAcceptorGroups),
			 numUnbatchers(numUnbatchers),
		batchers(config::F+1, config::BATCHER_PORT, WhoIsThis_Sender_client,
			    [&](const int socket, const Heartbeat& payload) {
			 	batchers.addHeartbeat(socket);
		}) {
    LOG("F: {}\n", config::F);
    std::thread server([&] {startServer(); });
    annaClient = anna::readWritable({}, [&](const std::string& key, const two_p_set& twoPSet) {
        batchers.connectAndMaybeListen(twoPSet);
    });
	annaClient->subscribeTo(config::KEY_BATCHERS);

    if (!isBenchmark)
	    readInput();
    else
	    benchmark();
    server.join();
}

[[noreturn]]
void paxos::startServer() {
    network::startServerAtPort<UnbatcherToClient>(config::CLIENT_PORT,
       [](const int socket) {
            BENCHMARK_LOG("Main connected to unbatcher\n");
    }, [&](const int socket, const UnbatcherToClient& payload) {
	    LOG("--Acked: {}--\n", payload.request());

	    if (isBenchmark) //send another message back immediately
		    batchers.send(message::createClientRequest(config::IP_ADDRESS, payload.request()));
	    else
		    readInput();
    });
}

void paxos::readInput() {
    printf("You may input...\n");
    std::string input;
    std::cin >> input;
    batchers.send(message::createClientRequest(config::IP_ADDRESS, input));
    TIME();
    printf("Waiting for ACK, do not input...\n");
}

void paxos::benchmark() {
	if (numBatchers > 0) {
		printf("Starting cluster, you should wait until a leader has been elected before starting...\n");
		startCluster();
	}

	printf("Enter any key to start benchmarking...\n");
	std::string input;
	std::cin >> input;

	for (int client = 0; client < numClients; client++) {
		//payload = client ID
		const std::string& payload = std::to_string(client);
		batchers.send(message::createClientRequest(config::IP_ADDRESS, payload));
	}
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
    if (argc != 1 && argc != 2 && argc != 6) {
        printf("Usage for interactive mode: ./Autoscaling_Paxos\n");
	    printf("Usage for benchmark mode without starting a new cluster: ./Autoscaling_Paxos <NUM CLIENTS>\n");
        printf("Usage for benchmark mode: ./Autoscaling_Paxos <NUM CLIENTS> <NUM BATCHERS> <NUM PROXY LEADERS> <NUM ACCEPTOR GROUPS> <NUM UNBATCHERS>\n");
        exit(0);
    }

	INIT_LOGGER();
	network::ignoreClosedSocket();

    if (argc == 1)
    	paxos p {};
    else {
	    const int numClients = std::stoi(argv[1]);
	    if (argc == 2) {
		    paxos p {numClients};
	    }
	    else {
		    const int numBatchers = std::stoi(argv[2]);
		    const int numProxyLeaders = std::stoi(argv[3]);
		    const int numAcceptorGroups = std::stoi(argv[4]);
		    const int numUnbatchers = std::stoi(argv[5]);
		    paxos p {numClients, numBatchers, numProxyLeaders, numAcceptorGroups, numUnbatchers};
	    }
    }
}