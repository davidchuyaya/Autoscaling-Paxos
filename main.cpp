
#include "main.hpp"

int main() {
    paxos p{};
}

[[noreturn]]
paxos::paxos() {
    std::cout << "F: " << config::F << std::endl;
    setbuf(stdout, nullptr); //TODO force flush to stdout. Disable when doing metrics or in prod
    connectToBatcher();
    readInput();
}

// TODO Connect to Multiple Batchers
void paxos::connectToBatcher() {
    printf("Input the IP Address of the Batcher to connect to: \n");
    std::string batcher_ip;
    std::cin >> batcher_ip;
    const int batcherSocketId = network::connectToServerAtAddress(batcher_ip, config::BATCHER_PORT);
    {std::lock_guard<std::mutex> lock(clientsMutex);
    clientSockets.push_back(batcherSocketId);}
}

[[noreturn]]
void paxos::readInput() {
    while (true) {
        std::string input;
        std::cin >> input;
        sendToBatcher(input);
    }
}

// TODO retry on timeout with different batcher
void paxos::sendToBatcher(const std::string& payload) {
    std::lock_guard<std::mutex> lock(clientsMutex);
    network::sendPayload(clientSockets[batcherIndex], payload);
    batcherIndex = (batcherIndex + 1) % (clientSockets.size());
}
