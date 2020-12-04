//
// Created by David Chu on 10/4/20.
//

#ifndef C__PAXOS_NETWORKNODE_HPP
#define C__PAXOS_NETWORKNODE_HPP

#include <csignal>
#include <netinet/in.h>
#include <google/protobuf/message.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
#include <functional>
#include <vector>
#include <tuple>
#include <optional>
#include <message.pb.h>
#include "config.hpp"
#include "../models/message.hpp"
#include "google/protobuf/util/delimited_message_util.h"

namespace network {
	void ignoreClosedSocket();
	/**
 * Creates a local server at the given port and listens.
 *
 * @param port Port of server
 * @return {Socket ID, socket address struct}
 */
	std::tuple<int, sockaddr_in> listenToPort(int port);

	/**
	 * Waits for a payload from the socket.
	 * @note Blocks until a payload arrives.
	 *
	 * @param socketId Socket ID of sender
	 * @return Payload
	 */
	bool receivePayload(google::protobuf::Message* message, google::protobuf::io::ZeroCopyInputStream* stream);

	template<typename Message>
	void listenToStream(const int socket, google::protobuf::io::ZeroCopyInputStream* inputStream,
	                    const std::function<void(int, const Message&)>& onPayloadReceived) {
		Message message;
		bool socketOpen = receivePayload(&message, inputStream);
		while (socketOpen) {
			onPayloadReceived(socket, message);
			message.Clear();
			socketOpen = receivePayload(&message, inputStream);
		}
	}

	/**
     * Creates a local server at the given port, waiting for clients to connect.
     * Triggers the callback function with a socket ID for each client connection.
     * @note Runs forever.
     *
     * @param port Port of server
     * @param onClientConnected Callback that accepts the socket ID of a client connection. Started in new thread, so
     * this is allowed to block
     */
	[[noreturn]]
	void startServerAtPortMultitype(int port, const std::function<void(int, const WhoIsThis_Sender&,
			google::protobuf::io::ZeroCopyInputStream*)>& onConnect);

	/**
     * Creates a local server at the given port, waiting for clients to connect.
     * Triggers the callback function with a socket ID for each client connection.
     * @note Runs forever.
     *
     * @param port Port of server
     * @param onClientConnected Callback that accepts the socket ID of a client connection. Started in new thread, so
     * this is allowed to block
     */
    template <typename Message>
    [[noreturn]]
    void startServerAtPort(const int port, const std::function<void(int)>& onConnect,
						   const std::function<void(int, const Message&)>& onPayloadReceived) {
		const auto& [socketId, serverAddress] = listenToPort(port);
		while (true) {
			socklen_t serverAddressSize = sizeof(serverAddress);
			const int socket = accept(socketId, (sockaddr *) &serverAddress, &serverAddressSize);
			std::thread thread([&, socket] {
				google::protobuf::io::ZeroCopyInputStream* inputStream = new google::protobuf::io::FileInputStream(socket);

				//parse whoIsThis
				WhoIsThis whoIsThis;
				bool socketOpen = receivePayload(&whoIsThis, inputStream);
				if (socketOpen) {
					onConnect(socket);

					//start receiving regular messages
					listenToStream(socket, inputStream, onPayloadReceived);
				}
				delete inputStream;
				close(socket);
			});
			thread.detach();
		}
	}

	//similar to startServerAtPort, but creates streams & closes the socket
	template <typename Message>
    void listenToSocketUntilClose(const int socket, const std::function<void(int, const Message&)>& onPayloadReceived) {
		google::protobuf::io::ZeroCopyInputStream* inputStream = new google::protobuf::io::FileInputStream(socket);
		listenToStream(socket, inputStream, onPayloadReceived);
		delete inputStream;
		close(socket);
	}

    /**
     * Creates a connection to the server at the given IP address and port.
     * @note Blocks until connection is made.
     *
     * @param address IP address of server
     * @param port Port of server
     * @return Socket ID
     */
    int connectToServerAtAddress(const std::string& address, int port, const WhoIsThis_Sender& identity);
    /**
     * Creates a TCP socket. Terminates on failure.
     *
     * @return Socket ID
     */
    int createSocket();
    /**
     * Sends the payload to the socket.
     * @param socketId Socket ID of recipient
     * @param payload Payload to send
     */
    void sendPayload(int socketId, const google::protobuf::Message& payload);
}

#endif //C__PAXOS_NETWORKNODE_HPP
