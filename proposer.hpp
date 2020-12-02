//
// Created by David Chu on 10/4/20.
//

#ifndef C__PAXOS_PROPOSER_HPP
#define C__PAXOS_PROPOSER_HPP

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <thread>
#include <algorithm>
#include <functional>
#include <shared_mutex>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <google/protobuf/message.h>

#include "utils/network.hpp"
#include "utils/config.hpp"
#include "models/message.hpp"
#include "message.pb.h"
#include "models/log.hpp"
#include "models/heartbeat_component.hpp"
#include "models/threshold_component.hpp"
#include "lib/storage/anna.hpp"

class proposer {
public:
    explicit proposer(int id, int numAcceptorGroups); //TODO once matchmakers are integrated, numAcceptorGroups is dynamic
private:
    const int id; // 0 indexed, no gaps
    const int numAcceptorGroups;
    anna* annaClient;

    std::shared_mutex ballotMutex;
    int ballotNum = 0; // must be at least 1 the first time it is sent

    LOGGER;

    std::atomic<bool> isLeader = false;
    std::shared_mutex heartbeatMutex;
    time_t lastLeaderHeartbeat = 0;

    std::shared_mutex remainingAcceptorGroupsForScoutsMutex;
    std::unordered_set<std::string> remainingAcceptorGroupsForScouts = {};

    std::shared_mutex logMutex;
    std::queue<int> logHoles = {};
    int nextSlot = 0;

    std::shared_mutex acceptorGroupLogsMutex;
    std::vector<Log::stringLog> acceptorGroupCommittedLogs = {};
    std::unordered_map<std::string, Log::pValueLog> acceptorGroupUncommittedLogs = {}; //key = acceptor group ID

    threshold_component proposers;

    two_p_set acceptorGroupIdSet;

    std::shared_mutex acceptorMutex;
    std::condition_variable_any acceptorCV;
    std::vector<std::string> acceptorGroupIds = {};
    int nextAcceptorGroup = 0;

    heartbeat_component proxyLeaders;

    /**
     * If isLeader = true, periodically tell other proposers.
     */
    [[noreturn]] void leaderLoop();

    void listenToAnna(const std::string& key, const two_p_set& twoPSet);
    [[noreturn]] void startServer();
    void listenToBatcher(const Batch& payload);
    void listenToProxyLeader(int socket, const ProxyLeaderToProposer& payload);
    void listenToProposer();

    /**
     * Broadcast p1a to acceptors to become the leader.
     * @invariant isLeader = false
     */
    void sendScouts();
    /**
     * Check if proxy leaders have replied with a win in phase 1.
     * If every acceptor group has replied, then we are the new leader, and we should merge the committed/uncommitted logs
     * of each acceptor group.
     * Otherwise, reset values.
     *
     * @invariant isLeader = false, shouldSendScouts = false, uncommittedProposals.empty()
     */
    void handleP1B(const ProxyLeaderToProposer& message);
    /**
     * Update log with newly committed slots from acceptors. Remove committed proposals from unproposedPayloads.
     * Propose uncommitted slots, add to uncommittedProposals
     * @invariant uncommittedProposals.empty()
     */
    void mergeLogs();
    /**
     * Check if a proxy leader has committed values for a slot. If yes, then confirm that slot as committed.
     * If we've been preempted, then that means another has become the leader. Reset values.
     * @invariant isLeader = true
     */
    void handleP2B(const ProxyLeaderToProposer& message);
    /**
     * Reset all values when this proposer learns that it is no longer the leader.
     */
    void noLongerLeader();
    /**
     * Increments (round robin) the next acceptor group a payload will be proposed to.
     * @warning Does NOT lock acceptorMutex. The caller MUST lock it.
     * @return The ID of the acceptor group to propose to.
     */
    const std::string& fetchNextAcceptorGroupId();
};


#endif //C__PAXOS_PROPOSER_HPP
