//
// Created by Taj Shaik on 10/15/20.
//

#include "batcher.hpp"

batcher::batcher(const int id, const std::map<int, std::string> proposers_addr) : id(id) {
    connectToProposers(proposers_addr);
    startServer();
}

[[noreturn]]
void batcher::startServer() {
    network::startServerAtPort(config::BATCHER_PORT, [&](const int clientSocketId) {
        printf("Batcher %d connected to client\n", id);
        listenToClient(clientSocketId);
    });
}

[[noreturn]]
void batcher::listenToClient(const int clientSocketId) {
    while (true) {
        std::string payload = network::receivePayload(clientSocketId);
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

// need to connect to the proposers
// have this information passed along to the proposers
void batcher::connectToProposers(const std::map<int, std::string> proposers_addr) {
    for (const auto pair : proposers_addr) {
        int i = pair.first;
        std::string proposer_ip_addr = pair.second;
        const int proposerPort = config::PROPOSER_PORT_START + i;
        threads.emplace_back(std::thread([&, proposerPort, i]{
            const int proposerSocketId = network::connectToServerAtAddress(proposer_ip_addr, proposerPort);
            network::sendPayload(proposerSocketId, message::createWhoIsThis(WhoIsThis_Sender_batcher));
            {std::lock_guard<std::mutex> lock(proposerMutex);
            proposerSockets.emplace_back(proposerSocketId);}
            printf("Batcher %d connected to proposer %d\n", id, i);
        }));
    }
}

int main(int argc, char** argv) {
    if(argc != 3) {
        printf("Please follow the format for running this function: ./batcher <BATCHER ID> <PROPOSER FILE NAME>.\n");
        exit(0);
    }
    int batcher_id = atoi( argv[1] );
    std::string proposer_file = argv[2];
    std::map<int, std::string> proposers = parser::parse_proposer(proposer_file);
    batcher(batcher_id, proposers);
}