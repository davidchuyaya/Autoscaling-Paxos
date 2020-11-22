//
// Created by David Chu on 10/4/20.
//

#include "network.hpp"
#include "../models/message.hpp"

[[noreturn]]
void network::startServerAtPort(const int port,
                                const std::function<void(int, const WhoIsThis_Sender&)>& onConnect,
                                const std::function<void(int, const WhoIsThis_Sender&, const std::string&)>&
                                        onPayloadReceived) {
    const auto& [socketId, serverAddress] = listenToPort(port);
    while (true) {
        socklen_t serverAddressSize = sizeof(serverAddress);
        const int socketToClient = accept(socketId, (sockaddr *) &serverAddress, &serverAddressSize);
        std::thread thread([&, socketToClient, onConnect] {
            //parse whoIsThis
            WhoIsThis whoIsThis;
            bool success = listenToSocket(socketToClient, [&whoIsThis, &onConnect]
            (const int socket, const std::string& payload) {
                whoIsThis.ParseFromString(payload);
                onConnect(socket, whoIsThis.sender());
            });
            if (!success) {
                close(socketToClient);
                return;
            }

            //start continuously listening
            listenToSocketUntilClose(socketToClient, [&whoIsThis, &onPayloadReceived]
            (int socket, const std::string& payload) {
                onPayloadReceived(socket, whoIsThis.sender(), payload);
            });
        });
        thread.detach();
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

bool network::listenToSocket(int socket, const std::function<void(int, const std::string&)>& onPayloadReceived) {
    const std::optional<std::string>& incoming = receivePayload(socket);
    if (incoming->empty())
        return false;
    //callback
    onPayloadReceived(socket, incoming.value());
    return true;
}

void network::listenToSocketUntilClose(int socket, const std::function<void(int, const std::string&)>& onPayloadReceived) {
    bool success = true;
    while (success)
        success = listenToSocket(socket, onPayloadReceived);
    close(socket);
}

int network::connectToServerAtAddress(const std::string& address, const int port, const WhoIsThis_Sender& identity) {
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

    //always send identity first
    sendPayload(socketId, message::createWhoIsThis(identity));

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
    sendPayload(socketId, serializedMessage);
}

void network::sendPayload(const int socketId, const std::string& payload) {
    uint16_t size = payload.length();
    int bytesWritten = write(socketId, &size, sizeof(size)); //send size first
    if (bytesWritten <= 0) {
        //TODO sender crashes if socket is closed
    }
    bytesWritten = write(socketId, payload.c_str(), payload.length());
    if (bytesWritten <= 0) {
        //bad return?
    }
}

std::optional<std::string> network::receivePayload(const int socketId) {
    uint16_t size = 0;
    auto bytesRead = read(socketId, &size, sizeof(uint16_t)); //read size first
    if (bytesRead <= 0)
        return {};

    std::string buffer(size, '\0');
    bytesRead = read(socketId, buffer.data(), size);
    if (bytesRead <= 0)
        return {};

    return buffer;
}