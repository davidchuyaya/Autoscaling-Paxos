//
// Created by David Chu on 11/11/20.
//

#include "heartbeat_component.hpp"

heartbeat_component::heartbeat_component(const int waitThreshold) : waitThreshold(waitThreshold) {
    std::thread thread([&]{checkHeartbeats();});
    thread.detach();
}

void heartbeat_component::connectToServers(const parser::idToIP& idToIPs, const int socketOffset,
                                                    const WhoIsThis_Sender& whoIsThis,
                                                    const std::function<void(int, const std::string&)>& listener) {
    for (const auto& idToIP : idToIPs) {
        const int id = idToIP.first;
        const std::string& ip = idToIP.second;

        std::thread thread([&, id, ip, whoIsThis, socketOffset, listener]{
            int socket = network::connectToServerAtAddress(ip, socketOffset + id, whoIsThis);
            addConnection(socket);
            network::listenToSocketUntilClose(socket, listener);
        });
        thread.detach();
    }
}

void heartbeat_component::addConnection(const int socket) {
    std::unique_lock lock(componentMutex);
    fastComponents.emplace_back(socket);
    //check threshold
    if (!thresholdMet())
        return;
    lock.unlock();
    componentCV.notify_one();
}

void heartbeat_component::waitForThreshold() {
    std::shared_lock lock(componentMutex);
    componentCV.wait(lock, [&]{return thresholdMet();});
    canSend = true;
}

bool heartbeat_component::thresholdMet() {
    return fastComponents.size() + slowComponents.size() >= waitThreshold;
}

int heartbeat_component::nextComponentSocket() {
    //prioritize sending to fast proxy leaders
    if (!fastComponents.empty()) {
        next = (next + 1) % fastComponents.size();
        return fastComponents[next];
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
        std::vector<int> slowedComponents = {};
        auto iterator = fastComponents.begin();
        while (iterator != fastComponents.end()) {
            const int socket = *iterator;
            if (difftime(now, heartbeats[socket]) > config::HEARTBEAT_TIMEOUT_SEC) {
                LOG("Node failed to heartbeat\n");
                slowComponents.emplace_back(socket);
                iterator = fastComponents.erase(iterator);
                slowedComponents.emplace_back(socket);
            }
            else
                ++iterator;
        }

        //if a node has a heartbeat, move it into the fast list
        iterator = slowComponents.begin();
        while (iterator != slowComponents.end()) {
            const int socket = *iterator;
            if (difftime(now, heartbeats[socket]) > config::HEARTBEAT_TIMEOUT_SEC) {
                fastComponents.emplace_back(socket);
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
