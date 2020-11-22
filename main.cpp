
#include "main.hpp"

[[noreturn]]
paxos::paxos() :
    batchers(config::F+1) {
    LOG("F: %d\n", config::F);
    const std::thread server([&] {startServer(); });
    anna annaClient({config::KEY_BATCHERS}, [&](const std::string& key, const two_p_set& twoPSet) {
        batchers.connectAndListen(twoPSet, config::BATCHER_PORT_START, WhoIsThis_Sender_client,
                                  [&](const int socket, const std::string& payload) {
            batchers.addHeartbeat(socket);
        });
    });
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

[[noreturn]]
void paxos::readInput() {
    while (true) {
        std::string input;
        std::cin >> input;
        //TODO do not resend until current value is acked
        const ClientToBatcher& request = message::createClientRequest(config::IP_ADDRESS, input);
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