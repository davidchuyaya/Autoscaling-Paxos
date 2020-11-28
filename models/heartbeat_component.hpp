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
#include <algorithm>
#include "lib/storage/two_p_set.hpp"
#include "message.pb.h"
#include "../utils/network.hpp"
#include "message.hpp"
#include "threshold_component.hpp"

class heartbeat_component : public threshold_component {
public:
    explicit heartbeat_component(int waitThreshold);
    void connectAndListen(const two_p_set& newMembers, int port, const WhoIsThis_Sender& whoIsThis,
                          const std::function<void(int, const std::string&)>& listener);
	void addConnection(int socket) override;
    template<typename Message> void send(const Message& payload) {
        std::shared_lock lock(componentMutex);
        if (!canSend) { //block if not enough connections
            waitForThreshold(lock);
        }
        int socket = nextComponentSocket();
        network::sendPayload(socket, payload);
    }
    void addHeartbeat(int socket);
private:
    std::shared_mutex heartbeatMutex;
    std::unordered_map<int, time_t> heartbeats = {}; //key = socket

    std::shared_mutex ipToSocketMutex;
    std::unordered_map<std::string, int> ipToSocket = {};

    std::vector<int> slowComponents = {};
    int next = 0;

    bool thresholdMet() override;
    int nextComponentSocket();
    [[noreturn]] void checkHeartbeats();

    using threshold_component::connectAndMaybeListen;
};


#endif //AUTOSCALING_PAXOS_HEARTBEAT_COMPONENT_HPP
