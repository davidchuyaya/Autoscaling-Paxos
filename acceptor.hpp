//
// Created by David Chu on 10/4/20.
//

#ifndef C__PAXOS_ACCEPTOR_HPP
#define C__PAXOS_ACCEPTOR_HPP

#include <string>
#include <vector>
#include "utils/config.hpp"
#include "utils/network.hpp"
#include "utils/metrics.hpp"
#include "models/log.hpp"
#include "models/message.hpp"
#include "models/server_component.hpp"
#include "message.pb.h"
#include "lib/storage/anna.hpp"

class acceptor {
public:
    explicit acceptor(std::string&& acceptorGroupId);
private:
	std::shared_ptr<metrics::variables> metricsVars;
	anna* annaClient;
	network *zmqNetwork;
	server_component* proxyLeaders;

    const std::string acceptorGroupId;
    Ballot highestBallot = {};
    Log::pValueLog log = {};

    /**
     * Process p1a and p2a messages from proxy leaders.
     * @param socket Socket ID of proxy leader
     */
    void listenToProxyLeaders(const network::addressPayloadsMap& addressToPayloads);
};

#endif //C__PAXOS_ACCEPTOR_HPP
