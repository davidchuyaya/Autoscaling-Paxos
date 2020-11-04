//
// Created by David Chu on 10/29/20.
//

#ifndef AUTOSCALING_PAXOS_PROXY_LEADER_HPP
#define AUTOSCALING_PAXOS_PROXY_LEADER_HPP


#include <vector>
#include <message.pb.h>
#include "utils/parser.hpp"
#include "models/log.hpp"

class proxy_leader {
public:
    explicit proxy_leader(int id, std::map<int, std::string> proposers, std::map<int, std::map<int, std::string>> acceptors);
private:
    const int id;
    std::mutex sentMessagesMutex;
    std::unordered_map<int, ProposerToAcceptor> sentMessages = {}; //key = message ID

    std::mutex unmergedLogsMutex;
    std::unordered_map<int, Log::acceptorGroupLog> unmergedLogs = {}; //key = message ID

    std::mutex approvedCommandersMutex;
    std::unordered_map<int, int> approvedCommanders = {}; //key = message ID

    std::mutex proposerMutex;
    std::unordered_map<int, int> proposerSockets = {}; //key = proposer ID

    std::mutex acceptorMutex;
    std::unordered_map<int, std::vector<int>> acceptorSockets = {}; //key = acceptor group ID
    std::vector<int> acceptorGroupIds = {};

    std::vector<std::thread> threads = {}; // A place to put threads so they don't get freed

    void connectToProposers(std::map<int, std::string> proposers);
    [[noreturn]] void listenToProposer(int socket);
    void connectToAcceptors(std::map<int, std::map<int, std::string>> acceptors);
    [[noreturn]] void listenToAcceptor(int socket);

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

    /**
     * Periodically send heartbeats to all proposers.
     */
    [[noreturn]] void sendHeartbeat();
};


#endif //AUTOSCALING_PAXOS_PROXY_LEADER_HPP
