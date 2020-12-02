//
// Created by Taj Shaik on 10/15/20.
//

#ifndef AUTOSCALING_PAXOS_BATCHER_HPP
#define AUTOSCALING_PAXOS_BATCHER_HPP

#include <shared_mutex>
#include <vector>
#include <thread>
#include <string>
#include <unordered_map>
#include <unistd.h>
#include <google/protobuf/message.h>

#include "utils/config.hpp"
#include "models/message.hpp"
#include "utils/network.hpp"
#include "utils/heartbeater.hpp"
#include "message.pb.h"
#include "lib/storage/anna.hpp"
#include "lib/storage/two_p_set.hpp"
#include "models/threshold_component.hpp"

class batcher {
public:
    explicit batcher();
private:
    anna* annaClient;

    std::shared_mutex payloadsMutex;
    std::unordered_map<std::string, std::string> clientToPayload = {};
    int numPayloads = 0;

    LOGGER;

    threshold_component proposers;

    std::shared_mutex clientMutex;
    std::vector<int> clientSockets = {};

    /**
     * Starts the server the clients connect to.
     * @note Runs forever.
     *
     */
    [[noreturn]] void startServer();
    /**
     * Listens to the clients.
     * @note Runs forever.
     *
     * @param client_address Address of the client
     */
    void listenToClient(const ClientToBatcher& payload);
};

#endif //AUTOSCALING_PAXOS_BATCHER_HPP
