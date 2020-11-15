//
// Created by Taj Shaik on 10/15/20.
//

#include "batcher.hpp"

batcher::batcher(const int id, const parser::idToIP& proposerIDtoIPs) : id(id) {
    connectToProposers(proposerIDtoIPs);
    const std::thread server([&] {startServer(); });
    heartbeater::heartbeat("i'm alive", clientMutex, clientSockets);
    pthread_exit(nullptr);
}

[[noreturn]]
void batcher::startServer() {
    network::startServerAtPort(config::BATCHER_PORT_START + id,
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

    std::shared_lock proposersLock(proposerMutex, std::defer_lock);
    std::scoped_lock lock(lastBatchTimeMutex, payloadsMutex, proposersLock);
    lastBatchTime = now;
    LOG("Sending batch\n");
    const Batch& batchMessage = message::createBatchMessage(clientToPayloads);
    for (const int socket : proposerSockets)
        network::sendPayload(socket, batchMessage);
    clientToPayloads.clear();
}

void batcher::connectToProposers(const parser::idToIP& proposerIDToIPs) {
    for (const auto& idToIP : proposerIDToIPs) {
        int proposerID = idToIP.first;
        std::string proposerIP = idToIP.second;
        const int proposerPort = config::PROPOSER_PORT_START + proposerID;

        std::thread thread([&, proposerIP, proposerPort] {
            const int proposerSocketId = network::connectToServerAtAddress(proposerIP, proposerPort, WhoIsThis_Sender_batcher);
            std::unique_lock lock(proposerMutex);
            proposerSockets.emplace_back(proposerSocketId);
            LOG("Batcher %d connected to proposer\n", id);
        });
        thread.detach();
    }
}

int main(const int argc, const char** argv) {
    if (argc != 3) {
        printf("Usage: ./batcher <BATCHER ID> <PROPOSER FILE NAME>.\n");
        exit(0);
    }
    const int batcherId = atoi(argv[1]);
    const std::string& proposerFile = argv[2];
    const parser::idToIP& proposers = parser::parseIDtoIPs(proposerFile);
    batcher(batcherId, proposers);
}