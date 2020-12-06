//
// Created by David Chu on 11/11/20.
//

#include "unbatcher.hpp"

unbatcher::unbatcher() {
	annaWriteOnlyClient = anna::writeOnly({{config::KEY_UNBATCHERS, config::IP_ADDRESS}});
    heartbeater::heartbeat(proxyLeaderMutex, proxyLeaders);
	startServer();
}

void unbatcher::startServer() {
    network::startServerAtPort<Batch>(config::UNBATCHER_PORT,
       [&](const int socket) {
           BENCHMARK_LOG("Unbatcher connected to proxy leader\n");
           std::unique_lock lock(proxyLeaderMutex);
           proxyLeaders.emplace_back(socket);
        }, [&](const int socket, const Batch& batch) {
        	LOG("Unbatcher received payload: {}\n", batch.ShortDebugString());
        	TIME();
        	for (const auto&[clientIp, request] : batch.clienttorequest()) {
		        //send payload
		        const int clientSocket = connectToClient(clientIp);
        		const bool sendSucceeded = network::sendPayload(clientSocket,
														  message::createUnbatcherToClientAck(request));

        		if (!sendSucceeded) {
        			//close socket. Note: Next time a client starts at the same IP, 1 message will be dropped first.
			        std::unique_lock lock(ipToSocketMutex);
			        close(clientSocket);
			        ipToSocket.erase(clientIp);
			        lock.unlock();
        		}
        	}
        	TIME();
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
    if (argc != 1) {
        printf("Usage: ./unbatcher\n");
        exit(0);
    }

    INIT_LOGGER();
	network::ignoreClosedSocket();
	unbatcher u {};
}