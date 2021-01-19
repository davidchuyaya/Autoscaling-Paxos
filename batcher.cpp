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
	}, [&](const network::addressPayloadsMap& addressToPayloads, const time_t now) {
		listenToProposers(addressToPayloads);
	});

	clients = new server_component(zmqNetwork, config::BATCHER_PORT_FOR_CLIENTS, Client,
						  [&](const std::string& address, const time_t now) {
		BENCHMARK_LOG("Client from {} connected to batcher", address);
	}, [&](const network::addressPayloadsMap& addressToPayloads, const time_t now) {
		listenToClients(addressToPayloads);
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

void batcher::listenToProposers(const network::addressPayloadsMap& addressToPayloads) {
	Ballot ballot;

	for (const auto&[address, payloads] : addressToPayloads) {
		for (const std::string& payload : payloads) {
			//hear new leader from proposer
			ballot.ParseFromString(payload);
			if (Log::isBallotGreaterThan(ballot, leaderBallot)) {
				BENCHMARK_LOG("New leader at batcher: {}", address);
				leaderBallot = ballot;
				leaderIP = address;
			}
			ballot.Clear();
		}
	}
}

void batcher::listenToClients(const network::addressPayloadsMap& addressToPayloads) {
	for (const auto&[address, payloads] : addressToPayloads) {
		metricsVars->counters[metrics::NumIncomingMessages]->Increment(payloads.size());
		for (const std::string& payload : payloads) {
			if (payload.empty())
				continue;

			LOG("Received --{}-- from client {}", payload, address);
			TIME();

			clientToPayloads[address] += payload + config::REQUEST_DELIMITER;
			numPayloads += 1;
			if (numPayloads >= config::BATCH_SIZE)
				sendBatch();
		}
	}
}

void batcher::sendBatch() {
	LOG("Sending batch");
	if (leaderIP.empty()) {
		for (const auto&[client, payloads] : clientToPayloads)
			proposers->broadcast(message::createBatchMessage(client, payloads).SerializeAsString());
	}
	else {
		for (const auto&[client, payloads] : clientToPayloads)
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