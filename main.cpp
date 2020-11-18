
#include "main.hpp"

[[noreturn]]
paxos::paxos() :
    batchers(config::F+1), annaClient([&](two_p_set& twoPSet){connectToBatchers(twoPSet);}) {
    LOG("F: %d\n", config::F);
#ifdef DEBUG
    setbuf(stdout, nullptr);
#endif
    const std::thread server([&] {startServer(); });
    annaClient.periodicGet2PSet(config::KEY_BATCHERS);
    readInput();
}

[[noreturn]]
void paxos::startServer() {
    network::startServerAtPort(config::CLIENT_PORT,
       [](const int socket, const WhoIsThis_Sender& whoIsThis) {
            LOG("Main connected to unbatcher\n");
    }, [](const int socket, const WhoIsThis_Sender& whoIsThis, const std::string& payload) {
            LOG("--Acked: {%s}--\n", payload.c_str());
    });
}

void paxos::connectToBatchers(two_p_set& twoPSet) {
    batchers.connectToServers(twoPSet, config::BATCHER_PORT_START, WhoIsThis_Sender_client,
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
        const ClientToBatcher& request = message::createClientRequest(network::getIp(), input);
        batchers.send(request);
    }
}

int main(const int argc, const char** argv) {
    if (argc != 1) {
        printf("Usage: ./Autoscaling_Paxos\n");
        exit(0);
    }
    paxos p {};
}