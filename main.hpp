//
// Created by David Chu on 10/4/20.
//

#ifndef C__PAXOS_MAIN_HPP
#define C__PAXOS_MAIN_HPP

#include <iostream>
#include <thread>
#include <shared_mutex>
#include <condition_variable>
#include <numeric>
#include <chrono>
#include "utils/config.hpp"
#include "acceptor.hpp"
#include "batcher.hpp"
#include "proposer.hpp"

class paxos {
public:
    explicit paxos(int numCommands);
private:
	const int numCommands;
	anna* annaClient;
    heartbeat_component batchers;

    std::shared_mutex requestMutex;
    std::condition_variable_any requestCV;
    std::optional<std::string> request;

    [[noreturn]] void startServer();
    [[noreturn]] void readInput();
    [[noreturn]] void resendInput();
	void benchmark();
};

#endif //C__PAXOS_MAIN_HPP
