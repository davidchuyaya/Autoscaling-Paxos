//
// Created by Taj Shaik on 10/15/20.
//

#include "batcher.hpp"

batcher::batcher() : proposers(config::F+1, config::PROPOSER_PORT, WhoIsThis_Sender_batcher) {
    annaClient = anna::readWritable({{config::KEY_BATCHERS, config::IP_ADDRESS}},
                    [&](const std::string& key, const two_p_set& twoPSet) {
    	proposers.connectAndMaybeListen(twoPSet);
    	if (proposers.twoPsetThresholdMet())
		    annaClient->unsubscribeFrom(config::KEY_PROPOSERS);
    });
	annaClient->subscribeTo(config::KEY_PROPOSERS);

    heartbeater::heartbeat(clientMutex, clientSockets);
    std::thread t([&]{ checkLaggingBatches(); });
    t.detach();
	startServer();
}

[[noreturn]]
void batcher::startServer() {
    network::startServerAtPort<ClientToBatcher>(config::BATCHER_PORT,
       [&](const int socket) {
           BENCHMARK_LOG("Connected to client\n");
           std::unique_lock lock(clientMutex);
           clientSockets.emplace_back(socket);
        }, [&](const int socket, const ClientToBatcher& payload) {
        	listenToClient(payload);
        });
}

void batcher::listenToClient(const ClientToBatcher& payload) {
    LOG("Received payload: {}\n", payload.request());
	TIME();

	std::unique_lock lock(payloadsMutex);
    clientToPayloads[payload.ipaddress()] += payload.request() + config::REQUEST_DELIMITER;
    numPayloads += 1;

	if (numPayloads < config::BATCH_SIZE)
		return;
	sendBatch();
}

void batcher::checkLaggingBatches() {
	while (true) {
		std::this_thread::sleep_for(std::chrono::seconds(config::BATCHER_TIMEOUT_SEC));

		std::unique_lock lock(payloadsMutex);
		if (numPayloads > 0) {
			BENCHMARK_LOG("Sending batch based on timeout, num payloads: {}", numPayloads);
			sendBatch();
		}
	}
}

void batcher::sendBatch() {
	LOG("Sending batch\n");
	for (const auto&[client, payloads] : clientToPayloads)
		proposers.broadcast(message::createBatchMessage(client, payloads));
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
	network::ignoreClosedSocket();
	batcher b {};
}