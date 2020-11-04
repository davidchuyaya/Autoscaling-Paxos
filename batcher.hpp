//
// Created by Taj Shaik on 10/15/20.
//

#ifndef AUTOSCALING_PAXOS_BATCHER_HPP
#define AUTOSCALING_PAXOS_BATCHER_HPP

#include <vector>
#include <thread>
#include <google/protobuf/message.h>
#include "utils/config.hpp"
#include "models/message.hpp"
#include "utils/network.hpp"
#include "utils/parser.hpp"
#include "message.pb.h"

class batcher {
public:
    explicit batcher(const int id, const std::map<int, std::string> proposers_addr);
private:
    int id = 0;
    std::vector<std::string> unproposedPayloads = {};
    std::vector<std::thread> threads = {}; // A place to put threads so they don't get freed

    std::mutex proposerMutex;
    std::vector<int> proposerSockets = {};

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
    [[noreturn]] void listenToClient(const int clientSocketId);
    /**
     * Connects to the Proposers.
     *
     * @param proposers_addr Address of the Proposers
     */
    void connectToProposers(const std::map<int, std::string> proposers_addr);
};

#endif //AUTOSCALING_PAXOS_BATCHER_HPP
