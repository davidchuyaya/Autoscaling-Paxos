//
// Created by David Chu on 10/4/20.
//

#ifndef C__PAXOS_MAIN_HPP
#define C__PAXOS_MAIN_HPP

#include <iostream>
#include <thread>
#include <mutex>
#include <numeric>

#include "utils/config.hpp"
#include "acceptor.hpp"
#include "batcher.hpp"
#include "proposer.hpp"

class paxos {
public:
    [[noreturn]] explicit paxos();
private:
    heartbeat_component batchers;

    [[noreturn]] void startServer();
    [[noreturn]] void readInput();
};

#endif //C__PAXOS_MAIN_HPP
