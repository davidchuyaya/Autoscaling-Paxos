//
// Created by David Chu on 10/4/20.
//

#ifndef C__PAXOS_MAIN_HPP
#define C__PAXOS_MAIN_HPP

#include <iostream>
#include <thread>
#include <numeric>

#include "utils/config.hpp"
#include "acceptor.hpp"
#include "batcher.hpp"
#include "proposer.hpp"

class paxos {
public:
    [[noreturn]] paxos();
private:
    int batcherIndex = 0;
    std::vector<std::thread> participants {};  // A place to put threads so they don't get freed
    std::mutex clientsMutex;
    std::vector<int> clientSockets {};

    void connectToBatcher();
    [[noreturn]] void readInput();
    void sendToBatcher(const std::string& payload);
};

#endif //C__PAXOS_MAIN_HPP
