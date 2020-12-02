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
    paxos(int numCommands, int numClients);
private:
	const int numCommands;
	const int numClients;
	const bool isBenchmark;
	anna* annaClient;
    heartbeat_component batchers;

    std::vector<std::shared_mutex> requestMutex;
    std::vector<std::condition_variable_any> requestCV;
    std::vector<std::optional<std::string>> request;

    [[noreturn]] void startServer();
    [[noreturn]] void readInput();
    [[noreturn]] void resendInput();
	void benchmark();
};

#endif //C__PAXOS_MAIN_HPP
