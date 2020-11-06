
#include "main.hpp"


int main(const int argc, const char** argv) {
    paxos p {};
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
    printf("Input the IP Address of the Batcher to connect to: ");
    std::string batcherIp;
    std::cin >> batcherIp;
    printf("Input the ID of the batcher: ");
    int batcherId;
    std::cin >> batcherId;
    const int batcherSocketId = network::connectToServerAtAddress(batcherIp, config::BATCHER_PORT_START + batcherId);
    {std::lock_guard<std::mutex> lock(batcherMutex);
    batcherSockets.push_back(batcherSocketId);}
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
    std::lock_guard<std::mutex> lock(batcherMutex);
    network::sendPayload(batcherSockets[batcherIndex], payload);
    batcherIndex = (batcherIndex + 1) % (batcherSockets.size());
}
