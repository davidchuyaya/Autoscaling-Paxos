//
// Created by David Chu on 10/4/20.
//

#ifndef C__PAXOS_ACCEPTOR_HPP
#define C__PAXOS_ACCEPTOR_HPP
#include <vector>
#include "models/log.hpp"
#include "message.pb.h"

class acceptor {
public:
    acceptor(int id);
private:
    const int id;
    std::mutex ballotMutex;
    Ballot highestBallot = {};
    std::mutex logMutex;
    std::vector<PValue> log = {};
    void startServer();
    [[noreturn]] void listenToProposer(int socket);
};


#endif //C__PAXOS_ACCEPTOR_HPP
