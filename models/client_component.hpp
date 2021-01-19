//
// Created by David Chu on 12/31/20.
//

#ifndef AUTOSCALING_PAXOS_CLIENT_COMPONENT_HPP
#define AUTOSCALING_PAXOS_CLIENT_COMPONENT_HPP

#include <string>
#include <memory>
#include <unordered_map>
#include <ctime>
#include "component.hpp"
#include "../utils/network.hpp"
#include "../lib/storage/anna.hpp"

class client_component: public component {
public:
	client_component(network* zmqNetwork, int port, ComponentType type, const onConnectHandler& onConnect,
				  const onConnectHandler& onDisconnect, const network::messageHandler& listener);
	void connectToNewMembers(const two_p_set& newMembers, time_t now);
	void removeConnection(const std::string& ipAddress, time_t now);
	void sendToIp(const std::string& ipAddress, const std::string& payload) override;
	void broadcast(const std::string& payload) override;
	int numConnections() const override;
	bool isConnected(const std::string& ipAddress) const override;
	std::unordered_set<std::string>& getAddresses() override;
private:
	const int port;
	const ComponentType type;
	const onConnectHandler onConnect;
	const onConnectHandler onDisconnect;
	std::unordered_set<std::string> clientAddresses;
	std::unordered_map<std::string, std::shared_ptr<socketInfo>> sockets;
	two_p_set members;
};


#endif //AUTOSCALING_PAXOS_CLIENT_COMPONENT_HPP
