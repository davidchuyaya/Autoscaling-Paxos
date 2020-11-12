//
// Created by Taj Shaik on 10/15/20.
//
#include <unistd.h>
#include "batcher.hpp"

batcher::batcher(const int id, const parser::idToIP& proposerIDtoIPs) : id(id) {
    connectToProposers(proposerIDtoIPs);
    startServer();
}

[[noreturn]]
void batcher::startServer() {
    network::startServerAtPort(config::BATCHER_PORT_START + id,
       [&](const int socket, const WhoIsThis_Sender& whoIsThis) {
            printf("Batcher %d connected to client\n", id);
        },
       [&](const int socket, const WhoIsThis_Sender& whoIsThis, const std::string& payloadString) {
            ClientToBatcher payload;
            payload.ParseFromString(payloadString);
            listenToClient(payload);
    });
}

void batcher::listenToClient(const ClientToBatcher& payload) {
    //first payload is IP address of client
    printf("Batcher %d received payload: [%s]\n", id, payload.request().c_str());
    {std::lock_guard<std::mutex> lock(payloadsMutex);
    clientToPayloads[payload.ipaddress()].emplace_back(payload.request());}

    //check if it's time to send another batch TODO separate timer in case client sends nothing else
    {std::lock_guard<std::mutex> lock(lastBatchTimeMutex);
    time_t now;
    time(&now);
    if (difftime(now, lastBatchTime) < config::BATCH_TIME_SEC)
        return;
    lastBatchTime = now;}

    std::scoped_lock lock(payloadsMutex, proposerMutex);
    printf("Sending batch\n");
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

        threads.emplace_back(std::thread([&, proposerIP, proposerPort] {
            const int proposerSocketId = network::connectToServerAtAddress(proposerIP, proposerPort, WhoIsThis_Sender_batcher);
            {std::lock_guard<std::mutex> lock(proposerMutex);
            proposerSockets.emplace_back(proposerSocketId);}
            printf("Batcher %d connected to proposer\n", id);
        }));
    }
}

int main(const int argc, const char** argv) {
    if (argc != 3) {
        printf("Usage: ./batcher <BATCHER ID> <PROPOSER FILE NAME>.\n");
        exit(0);
    }
    const int batcherId = atoi(argv[1] );
    const std::string& proposerFile = argv[2];
    const parser::idToIP& proposers = parser::parseIDtoIPs(proposerFile);
    batcher(batcherId, proposers);
}