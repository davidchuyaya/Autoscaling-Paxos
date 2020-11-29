
#include "main.hpp"

paxos::paxos(const int numCommands) : numCommands(numCommands), batchers(config::F+1) {
    LOG("F: %d\n", config::F);
    std::thread server([&] {startServer(); });
    server.detach();
    annaClient = new anna({config::KEY_BATCHERS}, [&](const std::string& key, const two_p_set& twoPSet) {
        batchers.connectAndListen(twoPSet, config::BATCHER_PORT, WhoIsThis_Sender_client,
                                  [&](const int socket, const std::string& payload) {
            batchers.addHeartbeat(socket);
        });
    });

    if (numCommands == 0) {
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
    network::startServerAtPort(config::CLIENT_PORT,
       [](const int socket, const WhoIsThis_Sender& whoIsThis) {
            LOG("Main connected to unbatcher\n");
    }, [&](const int socket, const WhoIsThis_Sender& whoIsThis, const std::string& payload) {
            printf("--Acked: {%s}--\n", payload.c_str());

            std::unique_lock lock(requestMutex);
            if (request.has_value()) {
            	if (request.value() == payload) {
		            request.reset();
		            lock.unlock();
		            requestCV.notify_all();
	            }
            	else {
		            LOG("Unexpected payload from unbatcher: {%s} when previous request was {%s}\n",
		                payload.c_str(), request.value().c_str());
	            }
            }
            else
	            LOG("Unexpected payload from unbatcher: {%s} when previous request DNE\n", payload.c_str());
    });
}

[[noreturn]]
void paxos::readInput() {
    while (true) {
        std::string input;
        std::cin >> input;
        batchers.send(message::createClientRequest(config::IP_ADDRESS, input));
	    auto start = std::chrono::system_clock::now();

	    std::unique_lock lock(requestMutex);
	    request.emplace(input);
	    LOG("Waiting for ACK, do not input...\n");
	    TIME();
	    requestCV.wait(lock, [&]{return !request.has_value();});
	    auto end = std::chrono::system_clock::now();

	    printf("Elapsed time %ld\n", std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());
	    TIME();
    }
}

[[noreturn]]
void paxos::resendInput() {
	std::string lastInput;
	while (true) {
		std::this_thread::sleep_for(std::chrono::seconds(config::CLIENT_TIMEOUT_SEC));

		std::shared_lock lock(requestMutex);
		if (!request.has_value())
			continue;

		if (lastInput.empty())
			lastInput = request.value();
		else if (lastInput == request.value()) //no response within timeout, resend
			batchers.send(message::createClientRequest(config::IP_ADDRESS, lastInput));
	}
}

void paxos::benchmark() {
	printf("Enter any key to start benchmarking...\n");
	std::string input;
	std::cin >> input;

	auto start = std::chrono::system_clock::now();

	const std::string& payload = "hi";
	const auto& protoMessage = message::createClientRequest(config::IP_ADDRESS, payload);
	for (int i = 0; i < numCommands; i++) {
		batchers.send(protoMessage);

		std::unique_lock lock(requestMutex);
		request.emplace(payload);
		LOG("Waiting for ACK, do not input...\n");
		requestCV.wait(lock, [&]{return !request.has_value();});
	}

	auto end = std::chrono::system_clock::now();

	printf("Elapsed time %ld for %d commands\n", std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count(),
		numCommands);
}

int main(const int argc, const char** argv) {
    if (argc != 2) {
        printf("Usage: ./Autoscaling_Paxos <NUM COMMANDS TO BENCHMARK, 0 FOR DEBUG MODE>\n");
        exit(0);
    }
	const int numCommands = std::stoi(argv[1]);
    paxos p {numCommands};
}