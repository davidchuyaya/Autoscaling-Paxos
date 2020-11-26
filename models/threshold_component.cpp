//
// Created by David Chu on 11/19/20.
//

#include "threshold_component.hpp"

threshold_component::threshold_component(const int waitThreshold) : waitThreshold(waitThreshold) {}

void threshold_component::connectAndMaybeListen(const two_p_set& newMembers, const int port,
                                                const WhoIsThis_Sender& whoIsThis,
                                                const std::optional<std::function<void(int, const std::string&)>>& listener) {
	std::unique_lock membersLock(membersMutex);
	const two_p_set& updates = members.updatesFrom(newMembers);
	if (updates.empty())
		return;
	members.merge(updates);
	membersLock.unlock();

	for (const std::string& ip : updates.getObserved()) {
        if (ip == config::IP_ADDRESS) //Don't connect to yourself
            continue;

        LOG("Connecting to new member: %s\n", ip.c_str());
        std::thread thread([&, ip, whoIsThis, port, listener]{
            const int socket = network::connectToServerAtAddress(ip, port, whoIsThis);
            std::unique_lock lock(ipToSocketMutex);
            ipToSocket[ip] = socket;
            lock.unlock();
            addConnection(socket);
            if (listener.has_value())
                network::listenToSocketUntilClose(socket, listener.value());
        });
        thread.detach();
    }

    if (!updates.getRemoved().empty()) {
        std::scoped_lock lock(ipToSocketMutex, componentMutex);
        for (const std::string& ip : updates.getRemoved()) {
            LOG("Removing dead member: %s\n", ip.c_str());
            const int socket = ipToSocket[ip];
            shutdown(socket, 1);
            components.erase(std::remove(components.begin(), components.end(), socket), components.end());
            ipToSocket.erase(ip);
        }
    }
}

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

