//
// Created by David Chu on 12/31/20.
//

#include "client_component.hpp"

client_component::client_component(network* zmqNetwork, int port, const ComponentType type,
								   const onConnectHandler& onConnect, const onConnectHandler& onDisconnect,
                                   const network::messageHandler& listener)
                                   : component(zmqNetwork), port(port), type(type), onConnect(onConnect),
                                   onDisconnect(onDisconnect) {
	zmqNetwork->addHandler(type, listener);
}

void client_component::connectToNewMembers(const two_p_set& newMembers, const time_t now) {
	const two_p_set& updates = members.updatesFrom(newMembers);
	if (updates.empty())
		return;
	members.merge(updates);

	for (const std::string& ip : updates.getObserved()) {
		if (ip == config::IP_ADDRESS) //Don't connect to yourself
			continue;
		sockets[ip] = zmqNetwork->connectToAddress(ip, port, type);
		clientAddresses.emplace(ip);
		onConnect(ip, now);
	}

	for (const std::string& ip : updates.getRemoved())
		removeConnection(ip, now);
}

void client_component::removeConnection(const std::string& ipAddress, const time_t now) {
	LOG("Removing dead member: {}", ipAddress);
	zmqNetwork->closeSocket(sockets[ipAddress]);
	sockets.erase(ipAddress);
	clientAddresses.erase(ipAddress);
	onDisconnect(ipAddress, now);
}

void client_component::sendToIp(const std::string& ipAddress, const std::string& payload) {
	if (ipAddress.empty())
		return;
	zmqNetwork->sendToServer(sockets[ipAddress]->socket, payload);
}

void client_component::broadcast(const std::string& payload) {
	LOG("Broadcasting payload: {}", payload);
	for (const auto& [ipAddress, socket] : sockets)
		zmqNetwork->sendToServer(socket->socket, payload);
}

int client_component::numConnections() const {
	return clientAddresses.size();
}

bool client_component::isConnected(const string& ipAddress) const {
	return clientAddresses.find(ipAddress) != clientAddresses.end();
}

std::unordered_set<std::string>& client_component::getAddresses() {
	return clientAddresses;
}