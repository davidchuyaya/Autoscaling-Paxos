//
// Created by David Chu on 12/31/20.
//

#include "server_component.hpp"

server_component::server_component(network* zmqNetwork, int port, const ComponentType type,
                                   const onConnectHandler& onConnect,
                                   const network::messageHandler& listener)
                                   : component(zmqNetwork), onConnect(onConnect), listener(listener) {
	zmqNetwork->addHandler(type,[&](const network::addressPayloadsMap& addressToPayload, const time_t now) {
		for (const auto&[address, payloads] : addressToPayload) {
			if (!isConnected(address)) {
				//new connection
				clientAddresses.emplace(address);
				this->onConnect(address, now); //store functions into variables so they can still be used when stack frame is cleared
			}
		}
		this->listener(addressToPayload, now);
	});
	serverSocket = zmqNetwork->startServerAtPort(port, type);
}

void server_component::sendToIp(const std::string& ipAddress, const std::string& payload) {
	if (ipAddress.empty())
		return;
	zmqNetwork->sendToClient(serverSocket->socket, ipAddress, payload);
}

void server_component::broadcast(const std::string& payload) {
	for (const std::string& ipAddress : clientAddresses)
		sendToIp(ipAddress, payload);
}

int server_component::numConnections() const {
	return clientAddresses.size();
}

bool server_component::isConnected(const std::string& ipAddress) const {
	return clientAddresses.find(ipAddress) != clientAddresses.end();
}
