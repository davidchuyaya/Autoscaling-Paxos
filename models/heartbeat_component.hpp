//
// Created by David Chu on 11/11/20.
//

#ifndef AUTOSCALING_PAXOS_HEARTBEAT_COMPONENT_HPP
#define AUTOSCALING_PAXOS_HEARTBEAT_COMPONENT_HPP

#include <unordered_map>
#include <vector>
#include <string>
#include <ctime>
#include <algorithm>
#include <functional>
#include "../utils/network.hpp"

class heartbeat_component {
public:
	explicit heartbeat_component(network* zmqNetwork);
	void addHeartbeat(const std::string& ipAddress, time_t now);
	void addConnection(const std::string& ipAddress, time_t now);
	void removeConnection(const std::string& ipAddress);
	//Note: returns "" if no components exist
	std::string nextAddress();
private:
	network* zmqNetwork;
    std::unordered_map<std::string, time_t> heartbeats;
	std::vector<std::string> fastComponents;
	std::vector<std::string> slowComponents;
    int next = 0;
    void checkHeartbeat(time_t now);
};


#endif //AUTOSCALING_PAXOS_HEARTBEAT_COMPONENT_HPP
