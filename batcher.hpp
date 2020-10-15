//
// Created by Taj Shaik on 10/15/20.
//

#ifndef AUTOSCALING_PAXOS_BATCHER_HPP
#define AUTOSCALING_PAXOS_BATCHER_HPP

#include <vector>
#include <deque>
#include <algorithm>
#include <thread>
#include <google/protobuf/message.h>
#include "utils/config.hpp"
#include "models/message.hpp"
#include "utils/network.hpp"
#include "message.pb.h"
#include "models/log.hpp"

class batcher {
public:
    explicit batcher(int id);
private:
    int id;
    
    std::vector<std::string> unproposedPayloads = {};

    std::mutex proposerMutex;
    std::vector<int> proposerSockets = {};

    [[noreturn]] void listenToMain();
    void connectToProposers();

    void broadcastToProposers(const google::protobuf::Message& message);

};

#endif //AUTOSCALING_PAXOS_BATCHER_HPP
