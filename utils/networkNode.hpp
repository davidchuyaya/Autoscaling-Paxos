//
// Created by David Chu on 10/4/20.
//

#ifndef C__PAXOS_NETWORKNODE_HPP
#define C__PAXOS_NETWORKNODE_HPP

#define MAX_CONNECTIONS 5

#include <netinet/in.h>

namespace network {
    /**
     * Creates a local server at the given port, waiting for clients to connect.
     * Triggers the callback function with a socket ID for each client connection.
     * Blocking.
     *
     * @param port Port of server
     * @param onClientConnected Callback that accepts the socket ID of a client connection
     */
    [[noreturn]] void startServerAtPort(int port, void(*onClientConnected)(int));
    /**
     * Creates a local server at the given port and listens.
     *
     * @param port Port of server
     * @return {Socket ID, socket address struct}
     */
    std::tuple<int, sockaddr_in> listenToPort(int port);
    /**
     * Creates a connection to the server at the given IP address and port.
     *
     * @param address IP address of server
     * @param port Port of server
     * @return Socket ID, or -1 if connection failed
     */
    int connectToServerAtAddress(const std::string& address, int port);
    /**
     * Creates a TCP socket. Terminates on failure.
     *
     * @return Socket ID
     */
    int createSocket();
};

class networkNode {
};


#endif //C__PAXOS_NETWORKNODE_HPP
