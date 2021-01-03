//
// Created by David Chu on 11/11/20.
//

#include "unbatcher.hpp"

unbatcher::unbatcher() {
//	annaWriteOnlyClient = anna::writeOnly({{config::KEY_UNBATCHERS, config::IP_ADDRESS}});
	zmqNetwork = new network();

	clients = new client_component(zmqNetwork, config::CLIENT_PORT_FOR_UNBATCHERS, Client,
	                           [](const std::string& address, const time_t now) {
		BENCHMARK_LOG("Unbatcher connected to client at {}", address);
	},[](const std::string& address, const time_t now) {
		BENCHMARK_LOG("ERROR??: Unbatcher disconnected from client at {}", address);
	}, [](const std::string& address, const std::string& payload, const time_t now) {
		BENCHMARK_LOG("ERROR??: Client at {} sent unbatcher --{}--", address, payload);
	});

	proxyLeaders = new server_component(zmqNetwork, config::UNBATCHER_PORT_FOR_PROXY_LEADERS, ProxyLeader,
	                              [&](const std::string& address, const time_t now) {
		BENCHMARK_LOG("Proxy leader from {} connected to unbatcher", address);
		proxyLeaders->sendToIp(address, ""); //send first heartbeat
	}, [&](const std::string& address, const std::string& payload, const time_t now) {
		Batch batch;
		batch.ParseFromString(payload);
		listenToProxyLeaders(batch);
		batch.Clear();
	});
	proxyLeaders->startHeartbeater();

	zmqNetwork->poll();
}

void unbatcher::listenToProxyLeaders(const Batch& batch) {
	LOG("Unbatcher received payload: {}", batch.ShortDebugString());
	TIME();

	if (!clients->isConnected(batch.client())) {
		clients->connectToNewMembers({{batch.client()},{}}, 0);
		BENCHMARK_LOG("Unbatcher connecting to client at {}", batch.client());
	}

	//split request
	std::stringstream stream(batch.request());
	std::string request;
	while (std::getline(stream, request, config::REQUEST_DELIMITER[0])) {
		LOG("Sending split request: {}", request);
		clients->sendToIp(batch.client(), request);
	}

	TIME();
}

int main(const int argc, const char** argv) {
    if (argc != 1) {
        printf("Usage: ./unbatcher\n");
        exit(0);
    }

    INIT_LOGGER();
	unbatcher u {};
}