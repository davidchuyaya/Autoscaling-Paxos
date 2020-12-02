//
// Created by David Chu on 11/11/20.
//

#include "heartbeat_component.hpp"

heartbeat_component::heartbeat_component(const int waitThreshold) : threshold_component(waitThreshold) {
    std::thread thread([&]{checkHeartbeats();});
    thread.detach();
}

void heartbeat_component::addConnection(const int socket) {
	{
		std::scoped_lock lock(componentMutex, heartbeatMutex);
		components.emplace_back(socket);
		//add 1st heartbeat immediately after connection is made
		LOG("Heartbeat added for socket: %d\n", socket);
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
