//
// Created by David Chu on 12/31/20.
//

#include "heartbeat_component.hpp"

heartbeat_component::heartbeat_component(network* zmqNetwork) : zmqNetwork(zmqNetwork) {}

void heartbeat_component::addHeartbeat(const std::string& ipAddress, const time_t now) {
	LOG("Added heartbeat from {}", ipAddress);
	heartbeats[ipAddress] = now;
}

void heartbeat_component::addConnection(const std::string& ipAddress, const time_t now) {
	fastComponents.emplace_back(ipAddress);
	zmqNetwork->addTimer([&, ipAddress](const time_t now) {
		checkHeartbeat(now, ipAddress);
	}, now, config::HEARTBEAT_TIMEOUT_SEC, true);
	addHeartbeat(ipAddress, now);
}

void heartbeat_component::removeConnection(const std::string& ipAddress) {
	fastComponents.erase(std::remove(fastComponents.begin(), fastComponents.end(), ipAddress), fastComponents.end());
	slowComponents.erase(std::remove(slowComponents.begin(), slowComponents.end(), ipAddress), slowComponents.end());
	heartbeats.erase(ipAddress);
}

std::string heartbeat_component::nextAddress() {
	//prioritize sending to fast proxy leaders
	if (!fastComponents.empty()) {
		next = (next + 1) % fastComponents.size();
		return fastComponents[next];
	}
	if (!slowComponents.empty()) {
		next = (next + 1) % slowComponents.size();
		return slowComponents[next];
	}
	return "";
}

void heartbeat_component::checkHeartbeat(const time_t now, const std::string& ipAddress) {
	auto iterator = fastComponents.begin();
	auto positionInFastComponents = std::find(fastComponents.begin(), fastComponents.end(), ipAddress);

	if (positionInFastComponents != fastComponents.end()) { //this is a fast component
		if (difftime(now, heartbeats[ipAddress]) > config::HEARTBEAT_TIMEOUT_SEC) {
			LOG("Node at {} failed to heartbeat\n", ipAddress);
			slowComponents.emplace_back(ipAddress);
			fastComponents.erase(positionInFastComponents);
		}
	}
	else { //this is a slow component
		if (difftime(now, heartbeats[ipAddress]) < config::HEARTBEAT_TIMEOUT_SEC) {
			LOG("Node at {} is fast again\n", ipAddress);
			auto positionInSlowComponents = std::find(slowComponents.begin(),
			                                          slowComponents.end(), ipAddress);
			fastComponents.emplace_back(ipAddress);
			slowComponents.erase(positionInSlowComponents);
		}
	}
}
