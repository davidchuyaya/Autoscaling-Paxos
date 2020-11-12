
#include "main.hpp"

[[noreturn]]
paxos::paxos(const parser::idToIP& batcherIdToIPs): batchers(config::F+1) {
    std::cout << "F: " << config::F << std::endl;
    setbuf(stdout, nullptr); //TODO force flush to stdout. Disable when doing metrics or in prod
    const std::thread server([&] {startServer(); });
    connectToBatchers(batcherIdToIPs);
    readInput();
}

[[noreturn]]
void paxos::startServer() {
    network::startServerAtPort(config::CLIENT_PORT,
       [](const int socket, const WhoIsThis_Sender& whoIsThis) {
            printf("Main connected to unbatcher\n");
    }, [](const int socket, const WhoIsThis_Sender& whoIsThis, const std::string& payload) {
            printf("--Acked: {%s}--\n", payload.c_str());
    });
}

void paxos::connectToBatchers(const parser::idToIP& batcherIdToIPs) {
    batchers.connectToServers(batcherIdToIPs, config::BATCHER_PORT_START, WhoIsThis_Sender_client,
    [&](const int socket, const std::string& payload) {
        batchers.addHeartbeat(socket);
    });
}

[[noreturn]]
void paxos::readInput() {
    while (true) {
        std::string input;
        std::cin >> input;
        //TODO replace localhost with IP address, retry on timeout with different batcher
        const ClientToBatcher& request = message::createClientRequest("127.0.0.1", input);
        batchers.send(request);
    }
}

int main(const int argc, const char** argv) {
    if (argc != 2) {
        printf("Usage: ./Autoscaling_Paxos <BATCHER FILE NAME>.\n");
        exit(0);
    }
    const std::string& batcherFileName = argv[1];
    const parser::idToIP& batchers = parser::parseIDtoIPs(batcherFileName);
    paxos p {batchers};
}