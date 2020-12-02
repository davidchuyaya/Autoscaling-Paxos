
#include "main.hpp"

paxos::paxos(const int numCommands, const int numClients) : numCommands(numCommands), numClients(numClients),
	isBenchmark(numCommands != 0), requestMutex(numClients), requestCV(numClients), request(numClients),
	batchers(config::F+1) {
    LOG("F: %d\n", config::F);
    std::thread server([&] {startServer(); });
    server.detach();
    annaClient = new anna({config::KEY_BATCHERS}, [&](const std::string& key, const two_p_set& twoPSet) {
        batchers.connectAndListen<Heartbeat>(twoPSet, config::BATCHER_PORT, WhoIsThis_Sender_client,
											 [&](const int socket, const Heartbeat& payload) {
            batchers.addHeartbeat(socket);
        });
    });

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
            LOG("Main connected to unbatcher\n");
    }, [&](const int socket, const UnbatcherToClient& payload) {
            LOG("--Acked: {%s}--\n", payload.request().c_str());

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
		            LOG("Unexpected payload from unbatcher: {%s} when previous request was {%s}\n",
		                payload.request().c_str(), request[requestIndex].value().c_str());
	            }
            }
            else
	            LOG("Unexpected payload from unbatcher: {%s} when previous request DNE\n", payload.request().c_str());
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
	    LOG("Waiting for ACK, do not input...\n");
	    TIME();
	    requestCV[0].wait(lock, [&]{return !request[0].has_value();});
	    auto end = std::chrono::system_clock::now();

	    printf("Elapsed micro %ld\n", std::chrono::duration_cast<std::chrono::microseconds>(end - start).count());
	    TIME();
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
			LOG("Batcher timed out, resent request: %s\n", lastInput.c_str());
		}
	}
}

void paxos::benchmark() {
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

	printf("Elapsed millis %ld for %d clients and %d commands\n",
		std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count(), numClients, numCommands);
}

int main(const int argc, const char** argv) {
    if (argc != 3) {
        printf("Usage: ./Autoscaling_Paxos <NUM COMMANDS, 0 FOR DEBUG MODE> <NUM CLIENTS, 1 FOR DEBUG MODE>\n");
        exit(0);
    }
	const int numCommands = std::stoi(argv[1]);
	const int numClients = std::stoi(argv[2]);
    paxos p {numCommands, numClients};
}