//
// Created by David Chu on 10/29/20.
//

#ifndef AUTOSCALING_PAXOS_PROXY_LEADER_HPP
#define AUTOSCALING_PAXOS_PROXY_LEADER_HPP


#include <vector>
#include <message.pb.h>
#include "models/log.hpp"

class proxy_leader {
public:
    explicit proxy_leader(int id);
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

    void connectToProposers();
    [[noreturn]] void listenToProposer(int socket);

    void connectToAcceptors();
    [[noreturn]] void listenToAcceptor(int socket);

    void handleP1B(const AcceptorToProxyLeader& payload);
    void handleP2B(const AcceptorToProxyLeader& payload);
};


#endif //AUTOSCALING_PAXOS_PROXY_LEADER_HPP
