//
// Created by David Chu on 10/4/20.
//

#ifndef C__PAXOS_MAIN_HPP
#define C__PAXOS_MAIN_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include "models/server_component.hpp"
#include "models/client_component.hpp"
#include "models/heartbeat_component.hpp"
#include "utils/config.hpp"
#include "utils/network.hpp"
#include "utils/uuid.hpp"
#include "utils/metrics.hpp"
#include "aws/scaling.hpp"

class paxos {
public:
	[[noreturn]]
	explicit paxos(int delay, int numClients, int numBatchers = 0, int numProxyLeaders = 0, int numAcceptorGroups = 0,
				   int numUnbatchers = 0);
private:
	const bool shouldStartCluster;
	const int numBatchers;
	const int numProxyLeaders;
	const int numAcceptorGroups;
	const int numUnbatchers;
	std::vector<string> instanceIdsOfBatchers;
	std::vector<string> instanceIdsOfProposers;
	std::vector<string> instanceIdsOfProxyLeaders;
	std::unordered_map<std::string, std::vector<string>> instanceIdsOfAcceptors;
	std::vector<string> instanceIdsOfUnbatchers;

	anna* annaClient;
	network* zmqNetwork;
	heartbeat_component* batcherHeartbeat;
	client_component* batchers;
	server_component* unbatchers;

	void startCluster();
};

#endif //C__PAXOS_MAIN_HPP
