//
// Created by David Chu on 12/31/20.
//

#ifndef AUTOSCALING_PAXOS_COMPONENT_HPP
#define AUTOSCALING_PAXOS_COMPONENT_HPP

#include <string>
#include <ctime>
#include <functional>
#include "../utils/network.hpp"
#include "../utils/component_types.hpp"

class component {
public:
	using onConnectHandler = std::function<void(const std::string& ipAddress, const time_t time)>;
	virtual void sendToIp(const std::string& ipAddress, const std::string& payload) = 0;
	virtual void broadcast(const std::string& payload) = 0;
	void startHeartbeater() {
		zmqNetwork->addTimer([&](const time_t now) {
			this->broadcast("");
		}, config::HEARTBEAT_SLEEP_SEC, true);
	}
	virtual int numConnections() const = 0;
	virtual bool isConnected(const std::string& ipAddress) const = 0;
protected:
	network* zmqNetwork;

	explicit component(network* zmqNetwork): zmqNetwork(zmqNetwork) {}
};


#endif //AUTOSCALING_PAXOS_COMPONENTS_HPP
