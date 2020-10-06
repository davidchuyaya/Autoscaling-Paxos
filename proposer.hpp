//
// Created by David Chu on 10/4/20.
//

#ifndef C__PAXOS_PROPOSER_HPP
#define C__PAXOS_PROPOSER_HPP


#include <vector>
#include "utils/networkNode.hpp"

class proposer: networkNode {
public:
    explicit proposer(int id);
private:
    const int id; // 0 indexed, no gaps
    std::mutex proposerMutex;
    std::vector<int> proposerSockets = {};
    std::mutex acceptorMutex;
    std::vector<int> acceptorSockets = {};
    std::vector<std::thread> threads = {}; // A place to put threads so they don't get freed

    [[noreturn]] void listenToMain();
    void startServer();
    void connectToProposers();
    [[noreturn]] void listenToProposers();
    void connectToAcceptors();
    [[noreturn]] void listenToAcceptors();
};


#endif //C__PAXOS_PROPOSER_HPP
