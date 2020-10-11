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
    explicit acceptor(int id);
private:
    const int id;
    std::mutex ballotMutex;
    Ballot highestBallot = {};
    std::mutex logMutex;
    std::vector<PValue> log = {};

    [[noreturn]] void startServer();
    /**
     * Process p1a and p2a messages from proposers.
     * @param socket Socket ID of proposer
     */
    [[noreturn]] void listenToProposer(int socket);
};


#endif //C__PAXOS_ACCEPTOR_HPP
