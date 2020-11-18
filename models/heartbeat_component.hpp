//
// Created by David Chu on 11/11/20.
//

#ifndef AUTOSCALING_PAXOS_HEARTBEAT_COMPONENT_HPP
#define AUTOSCALING_PAXOS_HEARTBEAT_COMPONENT_HPP

#include <shared_mutex>
#include <condition_variable>
#include <unordered_map>
#include <vector>
#include <thread>
#include <functional>
#include "lib/storage/two_p_set.hpp"
#include "message.pb.h"
#include "../utils/parser.hpp"
#include "../utils/network.hpp"
#include "message.hpp"

class heartbeat_component {
public:
    explicit heartbeat_component(int waitThreshold);
    void connectToServers(const parser::idToIP& idToIPs, int socketOffset, const WhoIsThis_Sender& whoIsThis,
                          const std::function<void(int, const std::string&)>& listener);
    void connectToServers(const two_p_set& newMembers, int socketOffset, const WhoIsThis_Sender& whoIsThis,
                          const std::function<void(int, const std::string&)>& listener);
    void addConnection(int socket);
    //TODO remove connection based on IP address (for remote). Store IP address of connections.
    template<typename Message> void send(const Message& payload) {
        std::shared_lock lock(componentMutex);
        if (!canSend) { //block if not enough connections
            waitForThreshold();
        }
        int socket = nextComponentSocket();
        network::sendPayload(socket, payload);
    }
    void addHeartbeat(int socket);
private:
    const int waitThreshold;

    std::shared_mutex heartbeatMutex;
    std::unordered_map<int, time_t> heartbeats = {}; //key = socket

    two_p_set members;
    std::unordered_map<std::string, int> ipToSocket = {};

    std::shared_mutex componentMutex;
    std::condition_variable_any componentCV;
    std::vector<int> fastComponents = {};
    std::vector<int> slowComponents = {};
    int next = 0;
    bool canSend = false;

    void waitForThreshold();
    bool thresholdMet();
    int nextComponentSocket();
    [[noreturn]] void checkHeartbeats();
};


#endif //AUTOSCALING_PAXOS_HEARTBEAT_COMPONENT_HPP
