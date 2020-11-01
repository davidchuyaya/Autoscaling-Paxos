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
            {std::lock_guard<std::mutex> lock(proposerMutex);
            for (const int socket : proposerSockets)
                network::sendPayload(socket, message::createBatchMessage(unproposedPayloads));}
            unproposedPayloads.clear();
        }
    }
}

void batcher::connectToProposers() {
    for (int i = 0; i < config::F + 1; i++) {
        const int proposerPort = config::PROPOSER_PORT_START + i;
        threads.emplace_back(std::thread([&, proposerPort, i]{
            const int proposerSocketId = network::connectToServerAtAddress(config::LOCALHOST, proposerPort);
            network::sendPayload(proposerSocketId, message::createWhoIsThis(WhoIsThis_Sender_batcher));
            {std::lock_guard<std::mutex> lock(proposerMutex);
            proposerSockets.emplace_back(proposerSocketId);}
            printf("Batcher %d connected to proposer %d\n", id, i);
        }));
    }
}