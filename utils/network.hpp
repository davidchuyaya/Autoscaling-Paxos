//
// Created by David Chu on 10/4/20.
//

#ifndef C__PAXOS_NETWORKNODE_HPP
#define C__PAXOS_NETWORKNODE_HPP

#define MAX_CONNECTIONS 5

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

namespace network {
    /**
     * Creates a local server at the given port, waiting for clients to connect.
     * Triggers the callback function with a socket ID for each client connection.
     * @note Runs forever.
     *
     * @param port Port of server
     * @param onClientConnected Callback that accepts the socket ID of a client connection. Started in new thread, so
     * this is allowed to block
     */
    [[noreturn]] void startServerAtPort(int port,
                                        const std::function<void(int, const WhoIsThis_Sender&)>& onConnect,
                                        const std::function<void(int, const WhoIsThis_Sender&, const std::string&)>&
                                                onPayloadReceived);
    /**
     * Creates a local server at the given port and listens.
     *
     * @param port Port of server
     * @return {Socket ID, socket address struct}
     */
    std::tuple<int, sockaddr_in> listenToPort(int port);

    bool listenToSocket(int socket, const std::function<void(int, const std::string&)>& onPayloadReceived);
    void listenToSocketUntilClose(int socket, const std::function<void(int, const std::string&)>& onPayloadReceived);
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
    void sendPayload(int socketId, const std::string& payload);
    /**
     * Waits for a payload from the socket.
     * @note Blocks until a payload arrives.
     *
     * @param socketId Socket ID of sender
     * @return Payload
     */
    std::optional<std::string> receivePayload(int socketId);

    std::string getIp();
}

#endif //C__PAXOS_NETWORKNODE_HPP
