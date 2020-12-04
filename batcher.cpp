//
// Created by Taj Shaik on 10/15/20.
//

#include "batcher.hpp"

batcher::batcher() : proposers(config::F+1) {
    annaClient = new anna(config::KEY_BATCHERS, {config::KEY_PROPOSERS},
                    [&](const std::string& key, const two_p_set& twoPSet) {
    	//template type doesn't matter, since we don't receive any messages from the proposer anyway
    	proposers.connectAndMaybeListen<Heartbeat>(twoPSet, config::PROPOSER_PORT, WhoIsThis_Sender_batcher, {});
    });
    heartbeater::heartbeat(clientMutex, clientSockets);
	startServer();
}

[[noreturn]]
void batcher::startServer() {
    network::startServerAtPort<ClientToBatcher>(config::BATCHER_PORT,
       [&](const int socket) {
           LOG("Connected to client\n");
           std::unique_lock lock(clientMutex);
           clientSockets.emplace_back(socket);
        }, [&](const int socket, const ClientToBatcher& payload) {
        	listenToClient(payload);
        });
}

void batcher::listenToClient(const ClientToBatcher& payload) {
    //first payload is IP address of client
    LOG("Received payload: {}\n", payload.request());
	TIME();

	std::unique_lock lock(payloadsMutex);
    clientToPayload[payload.ipaddress()] = payload.request();
    numPayloads += 1;

	if (numPayloads < config::THRESHOLD_BATCH_SIZE)
		return;

	LOG("Sending batch\n");
	const Batch& batchMessage = message::createBatchMessage(clientToPayload);
	proposers.broadcast(batchMessage);
	TIME();

	clientToPayload.clear();
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