//
// Created by David Chu on 10/4/20.
//

#ifndef C__PAXOS_ACCEPTOR_HPP
#define C__PAXOS_ACCEPTOR_HPP
#include <vector>
#include <mutex>
#include "models/log.hpp"
#include "utils/network.hpp"
#include "models/message.hpp"
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
    void listenToProxyLeaders(int socket);
    /**
     * Returns the log with only slots larger than the one provided.
     * @warning Does NOT lock logMutex. The caller MUST lock it
     * @param slotToFilter
     * @return
     */
    Log::pValueLog logAfterSlot(int slotToFilter);
};

#endif //C__PAXOS_ACCEPTOR_HPP
