//
// Created by David Chu on 10/4/20.
//

#ifndef C__PAXOS_ACCEPTOR_HPP
#define C__PAXOS_ACCEPTOR_HPP

#include <shared_mutex>
#include <vector>
#include <thread>

#include "utils/config.hpp"
#include "models/log.hpp"
#include "spdlog/spdlog.h"
#include "utils/network.hpp"
#include "models/message.hpp"
#include "message.pb.h"
#include "lib/storage/anna_write_only.hpp"

class acceptor {
public:
    explicit acceptor(std::string&& acceptorGroupId);
private:
    const std::string acceptorGroupId;
	anna_write_only* annaWriteOnlyClient;

	std::shared_mutex ballotMutex;
    Ballot highestBallot = {};

    std::shared_mutex logMutex;
    Log::pValueLog log = {};

    [[noreturn]] void startServer();
    /**
     * Process p1a and p2a messages from proxy leaders.
     * @param socket Socket ID of proxy leader
     */
    void listenToProxyLeaders(int socket, const ProposerToAcceptor& payload);
};

#endif //C__PAXOS_ACCEPTOR_HPP
