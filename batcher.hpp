//
// Created by Taj Shaik on 10/15/20.
//

#ifndef AUTOSCALING_PAXOS_BATCHER_HPP
#define AUTOSCALING_PAXOS_BATCHER_HPP

#include <string>
#include <unordered_map>
#include <unistd.h>
#include <google/protobuf/message.h>
#include "models/message.hpp"
#include "models/server_component.hpp"
#include "models/client_component.hpp"
#include "utils/config.hpp"
#include "utils/network.hpp"
#include "message.pb.h"
#include "lib/storage/anna.hpp"
#include "lib/storage/two_p_set.hpp"

class batcher {
public:
    explicit batcher();
private:
    anna* annaClient;
    network zmqNetwork;

    std::unordered_map<std::string, std::string> clientToPayloads = {};
    int numPayloads = 0;

    void sendBatch(client_component& proposers);
};

#endif //AUTOSCALING_PAXOS_BATCHER_HPP
