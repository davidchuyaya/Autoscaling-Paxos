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
    acceptor(int id, int acceptorGroupId);
private:
    const int id;
    const int acceptorGroupId;
    std::mutex ballotMutex;
    Ballot highestBallot = {};
    std::mutex logMutex;
    Log::pValueLog log = {};

    [[noreturn]] void startServer();
    /**
     * Process p1a and p2a messages from proxy leaders.
     * @param socket Socket ID of proxy leader
     */
    [[noreturn]] void listenToProxyLeaders(int socket);
};


#endif //C__PAXOS_ACCEPTOR_HPP
