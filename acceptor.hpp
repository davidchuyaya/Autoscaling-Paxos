//
// Created by David Chu on 10/4/20.
//

#ifndef C__PAXOS_ACCEPTOR_HPP
#define C__PAXOS_ACCEPTOR_HPP
#include <vector>
#include "models/log.hpp"
#include "message.pb.h"
#include "models/ballot.hpp"

class acceptor {
public:
    acceptor(int id);
private:
    const int id;
    std::mutex ballotMutex;
    ballot highestBallot = {0,0}; //id, ballotNum
    Log log;
    void startServer();
    [[noreturn]] void listenToProposer(int socket);
    ballot setAndReturnHighestBallot(ballot newBallot);
};


#endif //C__PAXOS_ACCEPTOR_HPP
