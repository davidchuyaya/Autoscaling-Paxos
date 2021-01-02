//
// Created by David Chu on 12/31/20.
//

#ifndef AUTOSCALING_PAXOS_SERVER_COMPONENT_HPP
#define AUTOSCALING_PAXOS_SERVER_COMPONENT_HPP

#include <string>
#include <memory>
#include <unordered_set>
#include <functional>
#include "component.hpp"
#include "../utils/network.hpp"

class server_component: public component {
public:
	server_component(network* zmqNetwork, int port, const ComponentType& type,
				  const onConnectHandler& onConnect, const network::messageHandler& listener);
	void sendToIp(const std::string& ipAddress, const std::string& payload) override;
	void broadcast(const std::string& payload) override;
	int numConnections() const override;
	bool isConnected(const std::string& ipAddress) const override;
private:
	std::shared_ptr<socketInfo> serverSocket;
	std::unordered_set<std::string> clientAddresses;
};


#endif //AUTOSCALING_PAXOS_SERVER_COMPONENT_HPP
