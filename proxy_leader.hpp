//
// Created by David Chu on 10/29/20.
//

#ifndef AUTOSCALING_PAXOS_PROXY_LEADER_HPP
#define AUTOSCALING_PAXOS_PROXY_LEADER_HPP

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <message.pb.h>
#include "utils/config.hpp"
#include "utils/network.hpp"
#include "utils/metrics.hpp"
#include "models/message.hpp"
#include "models/log.hpp"
#include "models/heartbeat_component.hpp"
#include "models/client_component.hpp"
#include "lib/storage/anna.hpp"

class proxy_leader {
public:
    explicit proxy_leader();
private:
	std::shared_ptr<metrics::variables> metricsVars;
	anna* annaClient;
	network* zmqNetwork;
	client_component* proposers;
	heartbeat_component* unbatcherHeartbeat;
	client_component* unbatchers;

    struct sentMetadata {
    	ProposerToAcceptor value;
    	std::string proposerAddress;
    };
    std::unordered_map<int, sentMetadata> sentMessages; //key = message ID
    std::unordered_map<int, Log::acceptorGroupLog> unmergedLogs; //key = message ID
    std::unordered_map<int, int> approvedCommanders; //key = message ID
    std::unordered_map<std::string, client_component*> acceptorGroups; //key = acceptor group ID
    std::unordered_set<std::string> connectedAcceptorGroups;
    two_p_set acceptorGroupIdSet;

    void listenToAnna(const std::string& key, const two_p_set& twoPSet, time_t now);
    void processNewAcceptorGroup(const std::string& acceptorGroupId);
    void listenToProposer(const network::addressPayloadsMap& addressToPayloads);
    void listenToAcceptor(const network::addressPayloadsMap& addressToPayloads);

    /**
     * Handle a p1b from an acceptor group.
     * If the acceptor preempted us, immediately tell the leader. Clear the value.
     * If the acceptor accepted the ballot, check if we have f+1 acceptors. If so, tell the leader & clear the value.
     *
     * @param payload
     */
    void handleP1B(const AcceptorToProxyLeader& payload);
    /**
     * Handle a p2b from an acceptor group.
     * If the acceptor preempted us, immediately tell the leader. Clear the value.
     * If the acceptor saved the message, check if we have f+1 acceptors. If so, tell the leader & clear the value.
     *
     * @param payload
     */
    void handleP2B(const AcceptorToProxyLeader& payload);
};


#endif //AUTOSCALING_PAXOS_PROXY_LEADER_HPP
