//
// Created by David Chu on 10/4/20.
//

#ifndef C__PAXOS_MAIN_HPP
#define C__PAXOS_MAIN_HPP

#include <iostream>
#include <thread>
#include <shared_mutex>
#include <condition_variable>
#include <string>
#include <numeric>
#include <chrono>

#include "utils/config.hpp"
#include "acceptor.hpp"
#include "batcher.hpp"
#include "proposer.hpp"
#include "aws/scaling.hpp"

class paxos {
public:
    paxos(int numCommands = 0, int numClients = 1, int numBatchers = 0, int numProxyLeaders = 0, int numAcceptorGroups = 0,
		  int numUnbatchers = 0);
private:
	const bool isBenchmark;
	const int numCommands;
	const int numClients;
	const int numBatchers;
	const int numProxyLeaders;
	const int numAcceptorGroups;
	const int numUnbatchers;

	anna* annaClient;
    heartbeat_component batchers;

	std::vector<std::shared_mutex> requestMutex;
    std::vector<std::condition_variable_any> requestCV;
    std::vector<std::optional<std::string>> request;

    [[noreturn]] void startServer();
    [[noreturn]] void readInput();
    [[noreturn]] void resendInput();

	void benchmark();
	void startCluster();
};

#endif //C__PAXOS_MAIN_HPP
