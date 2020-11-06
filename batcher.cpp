//
// Created by Taj Shaik on 10/15/20.
//

#include "batcher.hpp"

batcher::batcher(const int id, const parser::idToIP& proposerIDtoIPs) : id(id) {
    connectToProposers(proposerIDtoIPs);
    startServer();
}

[[noreturn]]
void batcher::startServer() {
    network::startServerAtPort(config::BATCHER_PORT_START + id, [&](const int clientSocketId) {
        printf("Batcher %d connected to client\n", id);
        listenToClient(clientSocketId);
    });
}

void batcher::listenToClient(const int clientSocketId) {
    while (true) {
        const std::optional<std::string>& incoming = network::receivePayload(clientSocketId);
        if (incoming->empty())
            return;

        const std::string& payload = incoming.value();
        printf("Batcher %d received payload: [%s]\n", id, payload.c_str());
        unproposedPayloads.emplace_back(payload);

        if (unproposedPayloads.size() >= config::THRESHOLD_BATCH_SIZE) {
            {std::lock_guard<std::mutex> lock(proposerMutex);
            for (const int socket : proposerSockets)
                network::sendPayload(socket, message::createBatchMessage(unproposedPayloads));}
            unproposedPayloads.clear();
        }
    }
}

void batcher::connectToProposers(const parser::idToIP& proposerIDToIPs) {
    for (const auto& idToIP : proposerIDToIPs) {
        int proposerID = idToIP.first;
        std::string proposerIP = idToIP.second;
        const int proposerPort = config::PROPOSER_PORT_START + proposerID;

        threads.emplace_back(std::thread([&, proposerIP, proposerPort] {
            const int proposerSocketId = network::connectToServerAtAddress(proposerIP, proposerPort);
            network::sendPayload(proposerSocketId, message::createWhoIsThis(WhoIsThis_Sender_batcher));
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
    const parser::idToIP& proposers = parser::parseProposer(proposerFile);
    batcher(batcherId, proposers);
}