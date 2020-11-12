
#include <unistd.h>
#include "main.hpp"

int main(const int argc, const char** argv) {
    paxos p {};
}


[[noreturn]]
paxos::paxos() {
    std::cout << "F: " << config::F << std::endl;
    setbuf(stdout, nullptr); //TODO force flush to stdout. Disable when doing metrics or in prod
    const std::thread server([&] {startServer(); });
    connectToBatcher();
    readInput();
}

[[noreturn]]
void paxos::startServer() {
    network::startServerAtPort(config::CLIENT_PORT,
       [](const int socket, const WhoIsThis_Sender& whoIsThis) {
            printf("Main connected to unbatcher");
    }, [](const int socket, const WhoIsThis_Sender& whoIsThis, const std::string& payload) {
            printf("--Acked: {%s}--\n", payload.c_str());
    });
}

// TODO Connect to Multiple Batchers
void paxos::connectToBatcher() {
    printf("Input the IP Address of the Batcher to connect to: ");
    std::string batcherIp;
    std::cin >> batcherIp;
    printf("Input the ID of the batcher: ");
    int batcherId;
    std::cin >> batcherId;
    const int batcherSocketId = network::connectToServerAtAddress(batcherIp, config::BATCHER_PORT_START + batcherId, WhoIsThis_Sender_client);
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
    //TODO replace localhost with IP address
    const ClientToBatcher& request = message::createClientRequest("127.0.0.1", payload);
    network::sendPayload(batcherSockets[batcherIndex], request);
    batcherIndex = (batcherIndex + 1) % (batcherSockets.size());
}
