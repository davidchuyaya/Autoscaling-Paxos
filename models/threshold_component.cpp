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

void threshold_component::waitForThreshold(std::shared_lock<std::shared_mutex>& lock) {
    componentCV.wait(lock, [&]{return thresholdMet();});
    canSend = true;
}

bool threshold_component::thresholdMet() {
    return components.size() + (addedSelfAsConnection ? 1 : 0) >= waitThreshold;
}

int threshold_component::socketForIP(const string& ipAddress) {
	std::shared_lock lock(ipToSocketMutex);
	return ipToSocket.at(ipAddress);
}

const two_p_set& threshold_component::getMembers() const {
    return members;
}

