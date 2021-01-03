//
// Created by Taj Shaik on 10/15/20.
//

#include "batcher.hpp"

batcher::batcher() {
	zmqNetwork = new network();

	proposers = new client_component(zmqNetwork, config::PROPOSER_PORT_FOR_BATCHERS, Proposer,
							[](const std::string& address, const time_t now) {
		BENCHMARK_LOG("Batcher connected to proposer at {}", address);
	},[](const std::string& address, const time_t now) {
		BENCHMARK_LOG("ERROR??: Proposer disconnected from batcher at {}", address);
	}, [](const std::string& address, const std::string& payload, const time_t now) {
		LOG("ERROR: Proposer {} sent payload --{}-- to batcher", address, payload);
	});
	proposers->connectToNewMembers({{"54.219.37.153", "13.52.215.70"},{}}, 0); //TODO add new members with anna

	clients = new server_component(zmqNetwork, config::BATCHER_PORT_FOR_CLIENTS, Client,
						  [&](const std::string& address, const time_t now) {
		BENCHMARK_LOG("Client from {} connected to batcher", address);
		clients->sendToIp(address, ""); //send first heartbeat
	}, [&](const std::string& address, const std::string& payload, const time_t now) {
		LOG("Received --{}-- from client {}", payload, address);
		TIME();
		clientToPayloads[address] += payload + config::REQUEST_DELIMITER;
		numPayloads += 1;
		if (numPayloads >= config::BATCH_SIZE)
			sendBatch();
	});
	clients->startHeartbeater();

	zmqNetwork->addTimer([&](const time_t now) {
		if (numPayloads > 0) {
			BENCHMARK_LOG("Sending batch based on timeout, num payloads: {}", numPayloads);
			sendBatch();
		}
	}, config::BATCHER_TIMEOUT_SEC, true);

	zmqNetwork->poll();

//    annaClient = anna::readWritable({{config::KEY_BATCHERS, config::IP_ADDRESS}},
//                    [&](const std::string& key, const two_p_set& twoPSet) {
//    	proposers.connectAndMaybeListen(twoPSet);
//    	if (proposers.twoPsetThresholdMet())
//		    annaClient->unsubscribeFrom(config::KEY_PROPOSERS);
//    });
//	annaClient->subscribeTo(config::KEY_PROPOSERS);
}

void batcher::sendBatch() {
	LOG("Sending batch");
	for (const auto&[client, payloads] : clientToPayloads)
		proposers->broadcast(message::createBatchMessage(client, payloads).SerializeAsString());
	TIME();
	clientToPayloads.clear();
	numPayloads = 0;
}

int main(const int argc, const char** argv) {
    if (argc != 1) {
        printf("Usage: ./batcher\n");
        exit(0);
    }

    INIT_LOGGER();
	batcher b {};
}