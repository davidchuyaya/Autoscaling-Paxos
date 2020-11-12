//
// Created by David Chu on 11/11/20.
//

#ifndef AUTOSCALING_PAXOS_HEARTBEAT_COMPONENT_HPP
#define AUTOSCALING_PAXOS_HEARTBEAT_COMPONENT_HPP

#include <mutex>
#include <unordered_map>
#include <vector>
#include <thread>
#include <functional>
#include "message.pb.h"
#include "../utils/parser.hpp"
#include "../utils/network.hpp"

class heartbeat_component {
public:
    explicit heartbeat_component(int waitThreshold);
    void connectToServers(const parser::idToIP& idToIPs, int socketOffset, const WhoIsThis_Sender& whoIsThis,
                          const std::function<void(int, const std::string&)>& listener);
    void addConnection(int socket);
    void waitForThreshold();
    template<typename Message> void send(const Message& payload) {
        std::lock_guard<std::mutex> lock(componentMutex);
        int socket = nextComponentSocket();
        network::sendPayload(socket, payload);
    }
    void addHeartbeat(int socket);
private:
    const int waitThreshold;

    std::mutex heartbeatMutex;
    std::unordered_map<int, time_t> heartbeats = {}; //key = socket

    std::mutex componentMutex;
    std::condition_variable componentCV;
    std::vector<int> fastComponents = {};
    std::vector<int> slowComponents = {};
    int next = 0;

    bool thresholdMet();
    int nextComponentSocket();
    [[noreturn]] void checkHeartbeats();
};


#endif //AUTOSCALING_PAXOS_HEARTBEAT_COMPONENT_HPP
