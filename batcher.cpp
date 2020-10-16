//
// Created by Taj Shaik on 10/15/20.
//

#include "batcher.hpp"

batcher::batcher(const int id) : id(id) {
    connectToProposers();
    listenToMain();
}

[[noreturn]]
void batcher::listenToMain() {
    const int serverSocket = network::connectToServerAtAddress(config::LOCALHOST, config::MAIN_PORT);
    printf("Batcher %d connected to main\n", id);

    while (true) {
        std::string payload = network::receivePayload(serverSocket);
        printf("Batcher %d received payload: [%s]\n", id, payload.c_str());

        unproposedPayloads.emplace_back(payload);

        if (unproposedPayloads.size() >= config::THRESHOLD_BATCH_SIZE) {
            const ProposerReceiver& proposerReceiver = message::createBatchMessage(unproposedPayloads);
            broadcastToProposers(proposerReceiver);
            unproposedPayloads.clear();
        }
    }
}

void batcher::broadcastToProposers(const google::protobuf::Message& message) {
    const std::string& serializedMessage = message.SerializeAsString();

    std::lock_guard<std::mutex> lock(proposerMutex);
    for (const int socket : proposerSockets) {
        network::sendPayload(socket, serializedMessage);
    }
}

void batcher::connectToProposers() {
    for (int i = 0; i < 2*config::F + 1; i++) {
        const int proposerPort = config::PROPOSER_PORT_START + i;
        threads.emplace_back(std::thread([&, proposerPort]{
            const int proposerSocketId = network::connectToServerAtAddress(config::LOCALHOST, proposerPort);
            {std::lock_guard<std::mutex> lock(proposerMutex);
            proposerSockets.emplace_back(proposerSocketId);}
            printf("Batcher %d connected to proposer %d\n", id, i);
        }));
    }
}