
#include "main.hpp"

paxos::paxos(const int numCommands, const int numClients, const int numBatchers,
			 const int numProxyLeaders, const int numAcceptorGroups, const int numUnbatchers) :
			 isBenchmark(numCommands != 0),  numCommands(numCommands), numClients(numClients), numBatchers(numBatchers),
			 numProxyLeaders(numProxyLeaders), numAcceptorGroups(numAcceptorGroups), numUnbatchers(numUnbatchers),
			 requestMutex(numClients), requestCV(numClients), request(numClients), batchers(config::F+1) {
    LOG("F: {}\n", config::F);
    std::thread server([&] {startServer(); });
    server.detach();
    annaClient = anna::readWritable({}, [&](const std::string& key, const two_p_set& twoPSet) {
        batchers.connectAndListen<Heartbeat>(twoPSet, config::BATCHER_PORT, WhoIsThis_Sender_client,
											 [&](const int socket, const Heartbeat& payload) {
            batchers.addHeartbeat(socket);
        });
    });
	annaClient->subscribeTo(config::KEY_BATCHERS);

    if (!isBenchmark) {
	    std::thread batchRetry([&] { resendInput(); });
	    batchRetry.detach();
	    readInput();
    }
    else {
    	//assuming batchers will never timeout during benchmarking
	    benchmark();
	    pthread_exit(nullptr);
    }
}

[[noreturn]]
void paxos::startServer() {
    network::startServerAtPort<UnbatcherToClient>(config::CLIENT_PORT,
       [](const int socket) {
            BENCHMARK_LOG("Main connected to unbatcher\n");
    }, [&](const int socket, const UnbatcherToClient& payload) {
            LOG("--Acked: {}--\n", payload.request());

            //payload = client ID
            const int requestIndex = isBenchmark ? std::stoi(payload.request()) : 0;

            std::unique_lock lock(requestMutex[requestIndex]);
            if (request[requestIndex].has_value()) {
            	if (request[requestIndex].value() == payload.request()) {
		            request[requestIndex].reset();
		            lock.unlock();
		            requestCV[requestIndex].notify_all();
	            }
            	else {
		            LOG("Unexpected payload from unbatcher: {} when previous request was {}\n",
		                payload.request(), request[requestIndex].value());
	            }
            }
            else
	            LOG("Unexpected payload from unbatcher: {} when previous request DNE\n", payload.request());
    });
}

[[noreturn]]
void paxos::readInput() {
    while (true) {
	    printf("You may input...\n");
        std::string input;
        std::cin >> input;
        batchers.send(message::createClientRequest(config::IP_ADDRESS, input));
	    auto start = std::chrono::system_clock::now();

	    std::unique_lock lock(requestMutex[0]);
	    request[0].emplace(input);
	    printf("Waiting for ACK, do not input...\n");
	    requestCV[0].wait(lock, [&]{return !request[0].has_value();});
	    auto end = std::chrono::system_clock::now();

	    printf("Elapsed micro %ld\n", std::chrono::duration_cast<std::chrono::microseconds>(end - start).count());
    }
}

[[noreturn]]
void paxos::resendInput() {
	std::string lastInput;
	while (true) {
		std::this_thread::sleep_for(std::chrono::seconds(config::CLIENT_TIMEOUT_SEC));

		std::shared_lock lock(requestMutex[0]);
		if (!request[0].has_value())
			continue;

		if (lastInput.empty())
			lastInput = request[0].value();
		else if (lastInput == request[0].value()) {//no response within timeout, resend
			batchers.send(message::createClientRequest(config::IP_ADDRESS, lastInput));
			LOG("Batcher timed out, resent request: {}\n", lastInput);
		}
	}
}

void paxos::benchmark() {
	if (numBatchers > 0) {
		printf("Starting cluster, you should wait until a leader has been elected before starting...\n");
		startCluster();
	}

	printf("Enter any key to start benchmarking...\n");
	std::string input;
	std::cin >> input;

	std::vector<std::thread> threads;
	threads.reserve(numClients);

	auto start = std::chrono::system_clock::now();

	for (int client = 0; client < numClients; client++) {
		threads.emplace_back([&, client]{
			//payload = client ID
			const std::string& payload = std::to_string(client);
			const ClientToBatcher& protoMessage = message::createClientRequest(config::IP_ADDRESS, payload);
			for (int i = 0; i < numCommands; i++) {
				batchers.send(protoMessage);

				std::unique_lock lock(requestMutex[client]);
				request[client].emplace(payload);
				LOG("Waiting for ACK, do not input...\n");
				requestCV[client].wait(lock, [&]{return !request[client].has_value();});
			}
		});
	}
	//wait for all threads to complete
	for (std::thread& t : threads)
		t.join();

	auto end = std::chrono::system_clock::now();

	BENCHMARK_LOG("Elapsed millis {} for {} clients and {} commands\n",
		std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count(), numClients, numCommands);
	printf("We're done\n");
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
    if (argc != 1 && argc != 3 && argc != 7) {
        printf("Usage for interactive mode: ./Autoscaling_Paxos\n");
	    printf("Usage for benchmark mode without starting a new cluster: ./Autoscaling_Paxos <NUM COMMANDS> <NUM CLIENTS>\n");
        printf("Usage for benchmark mode: ./Autoscaling_Paxos <NUM COMMANDS> <NUM CLIENTS> <NUM BATCHERS> <NUM PROXY LEADERS> <NUM ACCEPTOR GROUPS> <NUM UNBATCHERS>\n");
        exit(0);
    }

	INIT_LOGGER();
	network::ignoreClosedSocket();

    if (argc == 1)
    	paxos p {};
    else {
	    const int numCommands = std::stoi(argv[1]);
	    const int numClients = std::stoi(argv[2]);
	    if (argc == 3) {
		    paxos p {numCommands, numClients};
	    }
	    else {
		    const int numBatchers = std::stoi(argv[3]);
		    const int numProxyLeaders = std::stoi(argv[4]);
		    const int numAcceptorGroups = std::stoi(argv[5]);
		    const int numUnbatchers = std::stoi(argv[6]);
		    paxos p {numCommands, numClients, numBatchers, numProxyLeaders, numAcceptorGroups, numUnbatchers};
	    }
    }
}