//
// Created by David Chu on 11/11/20.
//

#include "heartbeat_component.hpp"
#include "../utils/network.hpp"
#include "message.hpp"

template<typename Message>
heartbeat_component<Message>::heartbeat_component(const int waitThreshold) : waitThreshold(waitThreshold) {
    std::thread thread([&]{checkHeartbeats();});
    thread.detach();
}

template<typename Message>
void heartbeat_component<Message>::connectToServers(const parser::idToIP& idToIPs, const int socketOffset,
                                                    const WhoIsThis_Sender& whoIsThis,
                                                    const std::function<void(const Message&)>* listener) {
    for (const auto& idToIP : idToIPs) {
        const int id = idToIP.first;
        const std::string& ip = idToIP.second;

        std::thread thread([&, id, ip, socketOffset]{
            int socket = network::connectToServerAtAddress(ip, socketOffset + id, whoIsThis);
            addConnection(socket);
            if (listener != nullptr)
                network::listenToSocket(socket, listener);
        });
        thread.detach();
    }
}

template<typename Message>
void heartbeat_component<Message>::addConnection(const int socket) {
    {std::lock_guard<std::mutex> lock(componentMutex);
    fastComponents.emplace_back(socket);
    //check threshold
    if (!thresholdMet())
        return;}
    componentCV.notify_one();
}

template<typename Message>
void heartbeat_component<Message>::waitForThreshold() {
    std::unique_lock lock(componentMutex);
    componentCV.wait(lock, [&]{return thresholdMet();});
}

template<typename Message>
bool heartbeat_component<Message>::thresholdMet() {
    return fastComponents.size() + slowComponents.size() >= waitThreshold;
}

template<typename Message>
void heartbeat_component<Message>::send(const Message& payload) {
    std::lock_guard<std::mutex> lock(componentMutex);
    int socket = nextComponentSocket();
    network::sendPayload(socket, payload);
}

template<typename Message>
int heartbeat_component<Message>::nextComponentSocket() {
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

template<typename Message>
void heartbeat_component<Message>::checkHeartbeats() {
    time_t now;
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(config::HEARTBEAT_TIMEOUT_SEC));

        std::scoped_lock lock(heartbeatMutex, componentMutex);
        time(&now);

        //if a node has no recent heartbeat, move it into the slow list
        std::vector<int> slowedComponents = {};
        auto iterator = fastComponents.begin();
        while (iterator != fastComponents.end()) {
            const int socket = *iterator;
            if (difftime(now, heartbeats[socket]) > config::HEARTBEAT_TIMEOUT_SEC) {
                printf("Node failed to heartbeat\n");
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

template<typename Message>
void heartbeat_component<Message>::addHeartbeat(int socket) {
    std::lock_guard<std::mutex> lock(heartbeatMutex);
    time(&heartbeats[socket]);
}
