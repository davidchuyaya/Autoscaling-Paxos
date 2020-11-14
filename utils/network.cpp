//
// Created by David Chu on 10/4/20.
//

#include "network.hpp"

[[noreturn]]
void network::startServerAtPort(const int port, const std::function<void(int)>& onClientConnected) {
    const auto& [socketId, serverAddress] = listenToPort(port);
    std::vector<std::thread> clientThreads {};
    while (true) {
        socklen_t serverAddressSize = sizeof(serverAddress);
        const int socketToClient = accept(socketId, (sockaddr *) &serverAddress, &serverAddressSize);
        clientThreads.emplace_back(std::thread(onClientConnected, socketToClient));
    }
}

std::tuple<int, sockaddr_in> network::listenToPort(const int port) {
    const int socketId = createSocket();

    sockaddr_in serverAddress {};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    const int bindResult = bind(socketId, (sockaddr*) &serverAddress, sizeof(serverAddress));
    if (bindResult < 0) {
        fprintf(stderr, "Server socket could not be created at port: %d", port);
        exit(1);
    }

    listen(socketId, MAX_CONNECTIONS);
    return {socketId, serverAddress};
}

int network::connectToServerAtAddress(const std::string& address, const int port) {
    int connectResult = -1;
    int socketId;
    while (connectResult < 0) {
        socketId = createSocket();

        sockaddr_in serverAddress {};
        serverAddress.sin_family = AF_INET;
        serverAddress.sin_port = htons(port);
        inet_pton(AF_INET, address.c_str(), &serverAddress.sin_addr);

        connectResult = connect(socketId, (sockaddr *) &serverAddress, sizeof(serverAddress));
        if (connectResult < 0) {
            std::this_thread::sleep_for(std::chrono::seconds(config::TCP_RETRY_TIMEOUT_SEC));
        }
//        close(socketId);
    }
    return socketId;
}

int network::createSocket() {
    const int socketId = socket(AF_INET, SOCK_STREAM, 0);
    if (socketId < 0) {
        perror("Socket could not be created");
        exit(1);
    }
    const int opt = 1;
    setsockopt(socketId, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));
    return socketId;
}

void network::sendPayload(const int socketId, const google::protobuf::Message& payload) {
    const std::string& serializedMessage = payload.SerializeAsString();
    write(socketId, serializedMessage.c_str(), serializedMessage.length());
}

void network::sendPayload(const int socketId, const std::string& payload) {
    write(socketId, payload.c_str(), payload.length());
}

std::optional<std::string> network::receivePayload(const int socketId) {
    //TODO a fixed read buffer size might come back to bite us when we send large messages
    char buffer[config::TCP_READ_BUFFER_SIZE] = {0};
    const auto size = read(socketId, buffer, config::TCP_READ_BUFFER_SIZE);
    if (size <= 0)
        return {};
    buffer[size] = '\0';
    return std::string(buffer);
}
