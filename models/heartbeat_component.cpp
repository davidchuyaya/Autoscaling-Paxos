//
// Created by David Chu on 11/11/20.
//

#include "heartbeat_component.hpp"

heartbeat_component::heartbeat_component(const int waitThreshold) : threshold_component(waitThreshold) {
    std::thread thread([&]{checkHeartbeats();});
    thread.detach();
}

void heartbeat_component::connectAndListen(const two_p_set& newMembers, const int port,
                                           const WhoIsThis_Sender& whoIsThis,
                                           const std::function<void(int, const std::string&)>& listener) {
	std::unique_lock membersLock(membersMutex);
	const two_p_set& updates = members.updatesFrom(newMembers);
	if (updates.empty())
		return;
	members.merge(updates);
	membersLock.unlock();

    for (const std::string& ip : updates.getObserved()) {
        LOG("Connecting to new member: %s\n", ip.c_str());
        std::thread thread([&, ip, whoIsThis, port, listener]{
            const int socket = network::connectToServerAtAddress(ip, port, whoIsThis);
            std::unique_lock lock(ipToSocketMutex);
            ipToSocket[ip] = socket;
            lock.unlock();
            heartbeat_component::addConnection(socket); //TODO figure out why virtual func isn't working normally
            network::listenToSocketUntilClose(socket, listener);
        });
        thread.detach();
    }

    if (!updates.getRemoved().empty()) {
        std::scoped_lock lock(ipToSocketMutex, componentMutex, heartbeatMutex);
        for (const std::string& ip : updates.getRemoved()) {
            LOG("Removing dead member: %s\n", ip.c_str());
            const int socket = ipToSocket[ip];
            shutdown(socket, 1);
            components.erase(std::remove(components.begin(), components.end(), socket), components.end());
            slowComponents.erase(std::remove(slowComponents.begin(), slowComponents.end(), socket), slowComponents.end());
            heartbeats.erase(socket);
            ipToSocket.erase(ip);
        }
    }
}

void heartbeat_component::addConnection(const int socket) {
	{
		std::scoped_lock lock(componentMutex, heartbeatMutex);
		components.emplace_back(socket);
		//add 1st heartbeat immediately after connection is made
		time(&heartbeats[socket]);

		//check threshold
		if (!thresholdMet())
			return;
	}
	componentCV.notify_all();
}

bool heartbeat_component::thresholdMet() {
    return components.size() + slowComponents.size() >= waitThreshold;
}

int heartbeat_component::nextComponentSocket() {
    //prioritize sending to fast proxy leaders
    if (!components.empty()) {
        next = (next + 1) % components.size();
        return components[next];
    }
    else {
        next = (next + 1) % slowComponents.size();
        return slowComponents[next];
    }
}

void heartbeat_component::checkHeartbeats() {
    time_t now;
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(config::HEARTBEAT_TIMEOUT_SEC));

        std::shared_lock heartbeatLock(heartbeatMutex, std::defer_lock);
        std::scoped_lock lock(heartbeatLock, componentMutex);
        time(&now);

        //if a node has no recent heartbeat, move it into the slow list
        auto iterator = components.begin();
        while (iterator != components.end()) {
            const int socket = *iterator;
            if (difftime(now, heartbeats[socket]) > config::HEARTBEAT_TIMEOUT_SEC) {
                LOG("Node failed to heartbeat\n");
                slowComponents.emplace_back(socket);
                iterator = components.erase(iterator);
            }
            else
                ++iterator;
        }

        //if a node has a heartbeat, move it into the fast list
        iterator = slowComponents.begin();
        while (iterator != slowComponents.end()) {
            const int socket = *iterator;
            if (difftime(now, heartbeats[socket]) < config::HEARTBEAT_TIMEOUT_SEC) {
                components.emplace_back(socket);
                iterator = slowComponents.erase(iterator);
            }
            else
                ++iterator;
        }
    }
}

void heartbeat_component::addHeartbeat(int socket) {
    std::unique_lock lock(heartbeatMutex);
    time(&heartbeats[socket]);
}
