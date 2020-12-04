//
// Created by David Chu on 11/19/20.
//

#include "threshold_component.hpp"

threshold_component::threshold_component(const int waitThreshold) : waitThreshold(waitThreshold) {}

void threshold_component::addConnection(const int socket) {
    std::unique_lock lock(componentMutex);
    components.emplace_back(socket);
    //check threshold
    if (!thresholdMet())
        return;
    lock.unlock();
    componentCV.notify_all();
}

void threshold_component::addSelfAsConnection() {
    addedSelfAsConnection = true;
}

void threshold_component::removeAll() {
	std::scoped_lock lock(ipToSocketMutex, componentMutex, membersMutex);
	for (const std::string& ip : members.getObserved()) {
		LOG("Removing dead member: {}\n", ip);
		const int socket = ipToSocket[ip];
		shutdown(socket, 1);
		components.erase(std::remove(components.begin(), components.end(), socket), components.end());
		ipToSocket.erase(ip);
	}
}

void threshold_component::waitForThreshold(std::shared_lock<std::shared_mutex>& lock) {
    componentCV.wait(lock, [&]{return thresholdMet();});
    canSend = true;
}

bool threshold_component::thresholdMet() const {
    return components.size() + (addedSelfAsConnection ? 1 : 0) >= waitThreshold;
}

int threshold_component::socketForIP(const string& ipAddress) {
	std::shared_lock lock(ipToSocketMutex);
	return ipToSocket.at(ipAddress);
}

bool threshold_component::twoPsetThresholdMet() {
	std::shared_lock lock(membersMutex);
	return members.getObserved().size() >= waitThreshold;
}