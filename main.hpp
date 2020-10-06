//
// Created by David Chu on 10/4/20.
//

#ifndef C__PAXOS_MAIN_HPP
#define C__PAXOS_MAIN_HPP

#include "proposer.hpp"
#include "acceptor.hpp"

class paxos {
public:
    paxos();
    [[noreturn]] void start();
private:
    std::vector<std::thread> participants {};  // A place to put threads so they don't get freed
    std::mutex clientsMutex;
    std::vector<int> clientSockets {};

    void startProposers();
    void startAcceptors();
    void startServer();
    [[noreturn]] void readInput();
    void broadcastToProposers(const std::string& payload);
};

#endif //C__PAXOS_MAIN_HPP
