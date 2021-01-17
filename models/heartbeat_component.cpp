//
// Created by David Chu on 12/31/20.
//

#include "heartbeat_component.hpp"

heartbeat_component::heartbeat_component(network* zmqNetwork) : zmqNetwork(zmqNetwork) {
	zmqNetwork->addTimer([&](const time_t now) {
		checkHeartbeat(now);
	}, config::HEARTBEAT_TIMEOUT_SEC, true);
}

void heartbeat_component::addHeartbeat(const std::string& ipAddress, const time_t now) {
	LOG("Added heartbeat from {}", ipAddress);
	heartbeats[ipAddress] = now;
}

void heartbeat_component::addConnection(const std::string& ipAddress, const time_t now) {
	fastComponents.emplace_back(ipAddress);
	addHeartbeat(ipAddress, now);
}

void heartbeat_component::removeConnection(const std::string& ipAddress) {
	fastComponents.erase(std::remove(fastComponents.begin(), fastComponents.end(), ipAddress), fastComponents.end());
	slowComponents.erase(std::remove(slowComponents.begin(), slowComponents.end(), ipAddress), slowComponents.end());
	heartbeats.erase(ipAddress);
}

std::string heartbeat_component::nextAddress() {
	//prioritize sending to fast proxy leaders
	next += 1;
	int modNext = next / config::ZMQ_MAX_READS_PER_SOCKET_PER_POLL; //usually use same component = faster, somehow
	if (!fastComponents.empty()) {
		if (modNext >= fastComponents.size()) {
			modNext = 0;
			next = 0;
		}
		return fastComponents[modNext];
	}
	if (!slowComponents.empty()) {
		if (modNext >= slowComponents.size()) {
			modNext = 0;
			next = 0;
		}
		return slowComponents[modNext];
	}
	return "";
}

void heartbeat_component::checkHeartbeat(const time_t now) {
	auto iterator = this->fastComponents.begin();
	while (iterator != this->fastComponents.end()) {
		const std::string ipAddress = *iterator;
		if (difftime(now, heartbeats[ipAddress]) > config::HEARTBEAT_TIMEOUT_SEC) {
			LOG("Node at {} failed to heartbeat", ipAddress);
			slowComponents.emplace_back(ipAddress);
			iterator = this->fastComponents.erase(iterator);
		}
		else
			++iterator;
	}

	//if a node has a heartbeat, move it into the fast list
	iterator = slowComponents.begin();
	while (iterator != slowComponents.end()) {
		const std::string ipAddress = *iterator;
		if (difftime(now, heartbeats[ipAddress]) < config::HEARTBEAT_TIMEOUT_SEC) {
			LOG("Node at {} is fast again", ipAddress);
			this->fastComponents.emplace_back(ipAddress);
			iterator = slowComponents.erase(iterator);
		}
		else
			++iterator;
	}
}
