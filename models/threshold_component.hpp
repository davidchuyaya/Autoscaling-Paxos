//
// Created by David Chu on 11/19/20.
//

#ifndef AUTOSCALING_PAXOS_THRESHOLD_COMPONENT_HPP
#define AUTOSCALING_PAXOS_THRESHOLD_COMPONENT_HPP

#include <shared_mutex>
#include <condition_variable>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <thread>
#include <functional>
#include <algorithm>
#include "lib/storage/two_p_set.hpp"
#include "message.pb.h"
#include "../utils/network.hpp"
#include "message.hpp"

class threshold_component {
public:
    explicit threshold_component(int waitThreshold);
    void connectAndMaybeListen(const two_p_set& newMembers, int port, const WhoIsThis_Sender& whoIsThis,
                               const std::optional<std::function<void(int, const std::string&)>>& listener);
    virtual void addConnection(int socket);
    void addSelfAsConnection();
    template<typename Message> void broadcast(const Message& payload) {
        std::shared_lock lock(componentMutex);
        if (!canSend) { //block if not enough connections
            waitForThreshold(lock);
        }
        for (int socket : components) {
            network::sendPayload(socket, payload);
        }
    }
    [[nodiscard]] int socketForIP(const std::string& ipAddress);
    [[nodiscard]] const two_p_set& getMembers() const;
protected:
    const int waitThreshold;

    std::shared_mutex membersMutex;
    two_p_set members;

    std::shared_mutex ipToSocketMutex;
    std::unordered_map<std::string, int> ipToSocket = {};

    std::shared_mutex componentMutex;
    std::condition_variable_any componentCV;
    std::vector<int> components = {};
    bool canSend = false;
    bool addedSelfAsConnection = false;

    void waitForThreshold(std::shared_lock<std::shared_mutex>& lock);
    virtual bool thresholdMet();
};


#endif //AUTOSCALING_PAXOS_THRESHOLD_COMPONENT_HPP
