//
// Created by Taj Shaik on 10/15/20.
//

#include "batcher.hpp"

batcher::batcher() {
	metricsVars = metrics::createMetricsVars({metrics::NumIncomingMessages, metrics::NumOutgoingMessages},{},{},{},
										  "batcher");

	zmqNetwork = new network();

    annaClient = anna::readWritable(zmqNetwork, {{config::KEY_BATCHERS, config::IP_ADDRESS}},
                    [&](const std::string& key, const two_p_set& twoPSet, const time_t now) {
    	proposers->connectToNewMembers(twoPSet, now);
    	if (proposers->numConnections() == config::F + 1) //heard from all proposers, don't expect change
		    annaClient->unsubscribeFrom(config::KEY_PROPOSERS);
    });
	annaClient->subscribeTo(config::KEY_PROPOSERS);

	proposers = new client_component(zmqNetwork, config::PROPOSER_PORT_FOR_BATCHERS, Proposer,
							[](const std::string& address, const time_t now) {
		BENCHMARK_LOG("Batcher connected to proposer at {}", address);
	},[](const std::string& address, const time_t now) {
		BENCHMARK_LOG("ERROR??: Proposer disconnected from batcher at {}", address);
	}, [&](const std::string& address, const std::string& payload, const time_t now) {
		//hear new leader from proposer
		Ballot ballot;
		ballot.ParseFromString(payload);
		if (Log::isBallotGreaterThan(ballot, leaderBallot)) {
			leaderBallot = ballot;
			leaderIP = address;
		}
	});

	clients = new server_component(zmqNetwork, config::BATCHER_PORT_FOR_CLIENTS, Client,
						  [&](const std::string& address, const time_t now) {
		BENCHMARK_LOG("Client from {} connected to batcher", address);
	}, [&](const std::string& address, const std::string& payload, const time_t now) {
		if (payload.empty())
			return;

		LOG("Received --{}-- from client {}", payload, address);
		TIME();
		metricsVars->counters[metrics::NumIncomingMessages]->Increment();
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
}

void batcher::sendBatch() {
	LOG("Sending batch");
	for (const auto&[client, payloads] : clientToPayloads) {
		if (leaderIP.empty()) //only send to the leader when one exists
			proposers->broadcast(message::createBatchMessage(client, payloads).SerializeAsString());
		else
			proposers->sendToIp(leaderIP, message::createBatchMessage(client, payloads).SerializeAsString());
	}
	metricsVars->counters[metrics::NumOutgoingMessages]->Increment(clientToPayloads.size());
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