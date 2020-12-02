//
// Created by David Chu on 10/4/20.
//

#include "network.hpp"

[[noreturn]]
void network::startServerAtPortMultitype(const int port, const std::function<void(int, const WhoIsThis_Sender&,
		google::protobuf::io::ZeroCopyInputStream*)>& onConnect) {
	const auto& [socketId, serverAddress] = listenToPort(port);
	while (true) {
		socklen_t serverAddressSize = sizeof(serverAddress);
		const int socket = accept(socketId, (sockaddr *) &serverAddress, &serverAddressSize);
		std::thread thread([&, socket, onConnect] {
			google::protobuf::io::ZeroCopyInputStream* inputStream = new google::protobuf::io::FileInputStream(socket);

			//parse whoIsThis
			WhoIsThis whoIsThis;
			bool socketOpen = receivePayload(&whoIsThis, inputStream);
			if (socketOpen) {
				onConnect(socket, whoIsThis.sender(), inputStream);
			}
			delete inputStream;
			close(socket);
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

    listen(socketId, config::SERVER_MAX_CONNECTIONS);
    return {socketId, serverAddress};
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
	google::protobuf::util::SerializeDelimitedToFileDescriptor(payload, socketId);
}

bool network::receivePayload(google::protobuf::Message* message, google::protobuf::io::ZeroCopyInputStream* stream) {
	bool empty = false;
	bool success = google::protobuf::util::ParseDelimitedFromZeroCopyStream(message, stream, &empty);
	return success && !empty;
}