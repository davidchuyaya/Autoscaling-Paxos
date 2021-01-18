//
// Created by Taj Shaik on 10/15/20.
//

#ifndef AUTOSCALING_PAXOS_BATCHER_HPP
#define AUTOSCALING_PAXOS_BATCHER_HPP

#include <string>
#include <unordered_map>
#include <unistd.h>
#include "models/message.hpp"
#include "models/server_component.hpp"
#include "models/client_component.hpp"
#include "utils/config.hpp"
#include "utils/network.hpp"
#include "utils/metrics.hpp"
#include "message.pb.h"
#include "lib/storage/anna.hpp"
#include "lib/storage/two_p_set.hpp"

class batcher {
public:
    explicit batcher();
private:
	std::shared_ptr<metrics::variables> metricsVars;
    anna* annaClient;
    network* zmqNetwork;
	client_component* proposers;
	server_component* clients;

	std::string leaderIP;
	Ballot leaderBallot;

    std::unordered_map<std::string, std::string> clientToPayloads = {};
    int numPayloads = 0;

	void listenToProposers(const network::addressPayloadsMap& addressToPayloads);
	void listenToClients(const network::addressPayloadsMap& addressToPayloads);
    void sendBatch();
};

#endif //AUTOSCALING_PAXOS_BATCHER_HPP
