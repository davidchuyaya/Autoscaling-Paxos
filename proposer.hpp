//
// Created by David Chu on 10/4/20.
//

#ifndef C__PAXOS_PROPOSER_HPP
#define C__PAXOS_PROPOSER_HPP


#include <vector>
#include <deque>
#include "utils/networkNode.hpp"
#include "message.pb.h"
#include "models/log.hpp"

class proposer {
public:
    explicit proposer(int id);
private:
    const int id; // 0 indexed, no gaps
    std::mutex ballotMutex;
    int ballotNum = 0;
    bool isLeader = false;

    bool shouldSendScouts = true;
    std::mutex scoutMutex;
    int numApprovedScouts = 0;
    int numPreemptedScouts = 0;

    std::mutex commanderMutex;
    std::unordered_map<int, int> slotToApprovedCommanders = {};
    std::unordered_map<int, int> slotToPreemptedCommanders = {};

    std::mutex unproposedPayloadsMutex;
    std::vector<std::string> unproposedPayloads = {};
    std::unordered_map<int, std::string> uncommittedProposals = {}; //invariant: empty until we are leader
    std::vector<std::string> log; //TODO don't store the entire log
    std::mutex acceptorLogsMutex;
    std::vector<std::vector<PValue>> acceptorLogs = {};

    std::mutex proposerMutex;
    std::vector<int> proposerSockets = {};
    std::mutex acceptorMutex;
    std::vector<int> acceptorSockets = {};
    std::vector<std::thread> threads = {}; // A place to put threads so they don't get freed

    [[noreturn]] void listenToMain();

    void startServer();
    void connectToProposers();
    void storeProposerSocket(int socket);
    [[noreturn]] void listenToProposer(int socket);

    void connectToAcceptors();
    void storeAcceptorSocket(int socket);
    [[noreturn]] void listenToAcceptor(int socket);
    void broadcastToAcceptors(const google::protobuf::Message& message);

    [[noreturn]] void mainLoop();
    void sendScouts();
    //invariant: isLeader = false
    void checkScouts();
    void sendCommandersForPayloads();
    void sendCommanders(int slot, const std::string &payload);
    void checkCommanders();
};


#endif //C__PAXOS_PROPOSER_HPP
