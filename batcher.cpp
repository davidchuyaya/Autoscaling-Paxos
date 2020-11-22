//
// Created by Taj Shaik on 10/15/20.
//

#include "batcher.hpp"

batcher::batcher(const int id) : id(id), proposers(config::F+1) {
    const std::thread server([&] {startServer(); });
    anna annaClient(config::KEY_BATCHERS, {config::KEY_PROPOSERS},
                    [&](const std::string& key, const two_p_set& twoPSet) {
                        proposers.connectAndMaybeListen(twoPSet, config::PROPOSER_PORT_START, WhoIsThis_Sender_batcher, {});
                    });

    heartbeater::heartbeat("i'm alive", clientMutex, clientSockets);
    pthread_exit(nullptr);
}

[[noreturn]]
void batcher::startServer() {
    network::startServerAtPort(config::BATCHER_PORT_START,
       [&](const int socket, const WhoIsThis_Sender& whoIsThis) {
           LOG("Batcher %d connected to client\n", id);
           std::unique_lock lock(clientMutex);
           clientSockets.emplace_back(socket);
        },
       [&](const int socket, const WhoIsThis_Sender& whoIsThis, const std::string& payloadString) {
            ClientToBatcher payload;
            payload.ParseFromString(payloadString);
            listenToClient(payload);
    });
}

void batcher::listenToClient(const ClientToBatcher& payload) {
    //first payload is IP address of client
    LOG("Batcher %d received payload: [%s]\n", id, payload.request().c_str());
    std::unique_lock payloadsLock(payloadsMutex);
    clientToPayloads[payload.ipaddress()].emplace_back(payload.request());
    payloadsLock.unlock();

    //check if it's time to send another batch TODO separate timer in case client sends nothing else
    std::shared_lock batchTimeLock(lastBatchTimeMutex);
    time_t now;
    time(&now);
    if (difftime(now, lastBatchTime) < config::BATCH_TIME_SEC)
        return;
    batchTimeLock.unlock();

    std::scoped_lock lock(lastBatchTimeMutex, payloadsMutex);
    lastBatchTime = now;
    LOG("Sending batch\n");
    const Batch& batchMessage = message::createBatchMessage(clientToPayloads);
    proposers.broadcast(batchMessage);
    clientToPayloads.clear();
}

int main(const int argc, const char** argv) {
    if (argc != 2) {
        printf("Usage: ./batcher <BATCHER ID>.\n");
        exit(0);
    }
    const int batcherId = std::stoi(argv[1]);
    batcher b = {batcherId};
}