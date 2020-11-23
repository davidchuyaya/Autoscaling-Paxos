//
// Created by David Chu on 10/29/20.
//

#ifndef AUTOSCALING_PAXOS_PROXY_LEADER_HPP
#define AUTOSCALING_PAXOS_PROXY_LEADER_HPP

#include <string>
#include <unordered_map>
#include <shared_mutex>
#include <condition_variable>
#include <vector>
#include <mutex>
#include <thread>
#include <message.pb.h>
#include "utils/config.hpp"
#include "utils/network.hpp"
#include "models/message.hpp"
#include "models/log.hpp"
#include "models/heartbeat_component.hpp"
#include "utils/heartbeater.hpp"
#include "lib/storage/anna.hpp"

class proxy_leader {
public:
    explicit proxy_leader(int id);
private:
    const int id;
    anna* annaClient;

    std::shared_mutex sentMessagesMutex;
    std::unordered_map<int, ProposerToAcceptor> sentMessages = {}; //key = message ID

    LOGGER;

    std::shared_mutex unmergedLogsMutex;
    std::unordered_map<int, Log::acceptorGroupLog> unmergedLogs = {}; //key = message ID

    std::shared_mutex approvedCommandersMutex;
    std::unordered_map<int, int> approvedCommanders = {}; //key = message ID

    threshold_component proposers;

    two_p_set acceptorGroupIdSet;

    std::shared_mutex acceptorMutex;
    std::unordered_map<std::string, threshold_component*> acceptorGroupSockets = {}; //key = acceptor group ID

    heartbeat_component unbatchers;

    void listenToAnna(const std::string& key, const two_p_set& twoPSet);
    void processAcceptorGroup(const two_p_set& twoPSet);
    void processAcceptors(const std::string& acceptorGroupId, const two_p_set& twoPSet);
    void listenToProposer(const ProposerToAcceptor& payload);
    void listenToAcceptor(const AcceptorToProxyLeader& payload);

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
	bool knowOfAcceptorGroup(const std::string& acceptorGroupId);
};


#endif //AUTOSCALING_PAXOS_PROXY_LEADER_HPP
