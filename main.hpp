//
// Created by David Chu on 10/4/20.
//

#ifndef C__PAXOS_MAIN_HPP
#define C__PAXOS_MAIN_HPP

#include "proposer.hpp"
#include "acceptor.hpp"

class paxos {
public:
    paxos(int f);
    [[noreturn]] void start();
private:
    const int f;
    std::vector<std::thread> participants {};

    void startProposers();
    void startAcceptors();
};

#endif //C__PAXOS_MAIN_HPP
