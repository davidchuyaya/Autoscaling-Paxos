//
// Created by David Chu on 10/29/20.
//

#ifndef AUTOSCALING_PAXOS_PROXY_LEADER_HPP
#define AUTOSCALING_PAXOS_PROXY_LEADER_HPP

#include <shared_mutex>
#include <condition_variable>
#include <vector>
#include <message.pb.h>
#include "utils/parser.hpp"
#include "models/log.hpp"
#include "models/heartbeat_component.hpp"

class proxy_leader {
public:
    explicit proxy_leader(int id, const parser::idToIP& unbatchers, const parser::idToIP& proposers,
                          const std::unordered_map<int, parser::idToIP>& acceptors);
private:
    const int id;
    std::shared_mutex sentMessagesMutex;
    std::unordered_map<int, ProposerToAcceptor> sentMessages = {}; //key = message ID

    std::shared_mutex unmergedLogsMutex;
    std::unordered_map<int, Log::acceptorGroupLog> unmergedLogs = {}; //key = message ID

    std::shared_mutex approvedCommandersMutex;
    std::unordered_map<int, int> approvedCommanders = {}; //key = message ID

    std::shared_mutex proposerMutex;
    std::unordered_map<int, int> proposerSockets = {}; //key = proposer ID

    std::shared_mutex acceptorMutex;
    std::condition_variable_any acceptorCV;
    std::unordered_map<int, std::vector<int>> acceptorSockets = {}; //key = acceptor group ID
    std::vector<int> acceptorGroupIds = {};

    heartbeat_component unbatchers;

    void connectToUnbatchers(const parser::idToIP& unbatchers);
    void connectToProposers(const parser::idToIP& proposers);
    void listenToProposer(const ProposerToAcceptor& payload);
    void connectToAcceptors(const std::unordered_map<int, parser::idToIP>& acceptors);
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
};


#endif //AUTOSCALING_PAXOS_PROXY_LEADER_HPP
