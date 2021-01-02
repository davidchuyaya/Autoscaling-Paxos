//
// Created by David Chu on 10/4/20.
//

#ifndef C__PAXOS_ACCEPTOR_HPP
#define C__PAXOS_ACCEPTOR_HPP

#include <string>
#include <vector>
#include "utils/config.hpp"
#include "utils/network.hpp"
#include "models/log.hpp"
#include "models/message.hpp"
#include "models/server_component.hpp"
#include "message.pb.h"
#include "lib/storage/anna.hpp"

class acceptor {
public:
    explicit acceptor(std::string&& acceptorGroupId);
private:
	network zmqNetwork;

    const std::string acceptorGroupId;
	anna* annaWriteOnlyClient;
    Ballot highestBallot = {};
    Log::pValueLog log = {};

    /**
     * Process p1a and p2a messages from proxy leaders.
     * @param socket Socket ID of proxy leader
     */
    void listenToProxyLeaders(const std::string& ipAddress, const ProposerToAcceptor& payload,
							  server_component& proxyLeaders);
};

#endif //C__PAXOS_ACCEPTOR_HPP
