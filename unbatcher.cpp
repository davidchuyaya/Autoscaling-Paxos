//
// Created by David Chu on 11/11/20.
//

#include <unistd.h>
#include "utils/network.hpp"
#include "unbatcher.hpp"

unbatcher::unbatcher(const int id) : id(id) {
    startServer();
}

void unbatcher::startServer() {
    network::startServerAtPort(config::UNBATCHER_PORT_START + id,
       [&](const int socket, const WhoIsThis_Sender& whoIsThis) {
            printf("Unbatcher %d connected to proxy leader\n", id);
        },
       [&](const int socket, const WhoIsThis_Sender& whoIsThis, const std::string& payload) {
       printf("Unbatcher received payload: %s\n", payload.c_str());
       Batch batch;
       batch.ParseFromString(payload);
       for (const auto&[clientIp, requests] : batch.clienttorequests()) {
           const int clientSocket = connectToClient(clientIp);
           for (const std::string& request : requests.requests())
               network::sendPayload(clientSocket, requests);
       }
    });
}

int unbatcher::connectToClient(const std::string& ipAddress) {
    std::unique_lock lock(ipToSocketMutex);
    int socket = ipToSocket[ipAddress]; //return the socket if connection already established
    if (socket != 0)
        return socket;

    lock.unlock(); //connect to server is blocking, unlock first
    socket = network::connectToServerAtAddress(ipAddress, config::CLIENT_PORT, WhoIsThis_Sender_unbatcher);
    lock.lock();
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