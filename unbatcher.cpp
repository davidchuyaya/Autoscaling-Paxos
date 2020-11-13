//
// Created by David Chu on 11/11/20.
//

#include "utils/network.hpp"
#include "utils/heartbeater.hpp"
#include "unbatcher.hpp"

unbatcher::unbatcher(const int id) : id(id) {
    const std::thread server([&] {startServer(); });
    heartbeater::heartbeat("i'm alive", proxyLeaderMutex, proxyLeaders);
    pthread_exit(nullptr);
}

void unbatcher::startServer() {
    network::startServerAtPort(config::UNBATCHER_PORT_START + id,
       [&](const int socket, const WhoIsThis_Sender& whoIsThis) {
           printf("Unbatcher %d connected to proxy leader\n", id);
           std::unique_lock lock(proxyLeaderMutex);
           proxyLeaders.emplace_back(socket);
        },
       [&](const int socket, const WhoIsThis_Sender& whoIsThis, const std::string& payload) {
           printf("Unbatcher received payload: %s\n", payload.c_str());
           Batch batch;
           batch.ParseFromString(payload);
           for (const auto&[clientIp, requests] : batch.clienttorequests()) {
               const int clientSocket = connectToClient(clientIp); //TODO first message sent after a connection is set up is always lost
               for (const std::string& request : requests.requests())
                   network::sendPayload(clientSocket, request);
           }
    });
}

int unbatcher::connectToClient(const std::string& ipAddress) {
    std::unique_lock lock(ipToSocketMutex); //writeLock, since we don't want 2 ppl creating new sockets for the same client
    int socket = ipToSocket[ipAddress]; //return the socket if connection already established
    if (socket != 0)
        return socket;

    socket = network::connectToServerAtAddress(ipAddress, config::CLIENT_PORT, WhoIsThis_Sender_unbatcher);
    ipToSocket[ipAddress] = socket;
    return socket;
}

int main(const int argc, const char** argv) {
    if (argc != 2) {
        printf("Usage: ./unbatcher <UNBATCHER ID>.\n");
        exit(0);
    }
    const int id = atoi(argv[1]);
    unbatcher {id};
}