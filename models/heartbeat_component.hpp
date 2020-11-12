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

template<typename Message>
class heartbeat_component {
public:
    explicit heartbeat_component(int waitThreshold);
    void connectToServers(const parser::idToIP& idToIPs, int socketOffset, const WhoIsThis_Sender& whoIsThis,
                          const std::function<void(const Message&)>* listener);
    void addConnection(int socket);
    void waitForThreshold();
    void send(const Message& payload);
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
