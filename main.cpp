
#include "main.hpp"

[[noreturn]]
paxos::paxos() : batchers(config::F+1) {
    LOG("F: %d\n", config::F);
    const std::thread server([&] {startServer(); });
    annaClient = new anna({config::KEY_BATCHERS}, [&](const std::string& key, const two_p_set& twoPSet) {
        batchers.connectAndListen(twoPSet, config::BATCHER_PORT_START, WhoIsThis_Sender_client,
                                  [&](const int socket, const std::string& payload) {
            batchers.addHeartbeat(socket);
        });
    });
    readInput();
}

[[noreturn]]
void paxos::startServer() {
    network::startServerAtPort(config::CLIENT_PORT,
       [](const int socket, const WhoIsThis_Sender& whoIsThis) {
            LOG("Main connected to unbatcher\n");
    }, [&](const int socket, const WhoIsThis_Sender& whoIsThis, const std::string& payload) {
            LOG("--Acked: {%s}--\n", payload.c_str());

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

	    std::unique_lock lock(requestMutex);
	    request.emplace(input);
	    LOG("Waiting for ACK, do not input...\n");
	    requestCV.wait(lock, [&]{return request->empty();});
    }
}

int main(const int argc, const char** argv) {
    if (argc != 1) {
        printf("Usage: ./Autoscaling_Paxos\n");
        exit(0);
    }
    paxos p {};
}