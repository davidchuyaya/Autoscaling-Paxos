//
// Created by Taj Shaik on 10/15/20.
//

#ifndef AUTOSCALING_PAXOS_BATCHER_HPP
#define AUTOSCALING_PAXOS_BATCHER_HPP

#include <shared_mutex>
#include <vector>
#include <thread>
#include <mutex>
#include <google/protobuf/message.h>
#include "utils/config.hpp"
#include "models/message.hpp"
#include "utils/network.hpp"
#include "utils/parser.hpp"
#include "message.pb.h"

class batcher {
public:
    explicit batcher(int id, const parser::idToIP& proposerIDtoIPs);
private:
    int id = 0;
    std::shared_mutex lastBatchTimeMutex;
    time_t lastBatchTime = 0;

    std::shared_mutex payloadsMutex;
    std::unordered_map<std::string, std::vector<std::string>> clientToPayloads = {};

    std::shared_mutex proposerMutex;
    std::vector<int> proposerSockets = {};

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
    void connectToProposers(const parser::idToIP& proposerIDtoIPs);
};

#endif //AUTOSCALING_PAXOS_BATCHER_HPP
