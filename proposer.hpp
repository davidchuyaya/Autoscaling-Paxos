//
// Created by David Chu on 10/4/20.
//

#ifndef C__PAXOS_PROPOSER_HPP
#define C__PAXOS_PROPOSER_HPP


#include <vector>
#include "utils/networkNode.hpp"
#include "message.pb.h"

class proposer {
public:
    explicit proposer(int id);
private:
    const int id; // 0 indexed, no gaps
    int ballotNum = 0;
    bool isLeader = false;
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
};


#endif //C__PAXOS_PROPOSER_HPP
