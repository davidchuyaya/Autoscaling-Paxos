//
// Created by David Chu on 10/4/20.
//

#ifndef C__PAXOS_PROPOSER_HPP
#define C__PAXOS_PROPOSER_HPP

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <queue>
#include <algorithm>
#include <functional>
#include "message.pb.h"
#include "utils/network.hpp"
#include "utils/config.hpp"
#include "utils/metrics.hpp"
#include "models/message.hpp"
#include "models/log.hpp"
#include "models/client_component.hpp"
#include "models/server_component.hpp"
#include "models/heartbeat_component.hpp"
#include "lib/storage/anna.hpp"

class proposer {
public:
    explicit proposer(int id, int numAcceptorGroups); //TODO once matchmakers are integrated, numAcceptorGroups is dynamic
private:
    const int id; // 0 indexed, no gaps
    const int numAcceptorGroups;

    std::shared_ptr<metrics::variables> metricsVars;
    anna* annaClient;
	network* zmqNetwork;
	client_component* proposers;
	heartbeat_component* proxyLeaderHeartbeat;
	server_component* proxyLeaders;
	server_component* batchers;

	int ballotNum = 0; // must be at least 1 the first time it is sent
	Ballot ballot;
    bool isLeader = false;
    time_t lastLeaderHeartbeat = 0;

    std::unordered_set<std::string> remainingAcceptorGroupsForScouts = {};

    std::queue<int> logHoles = {};
    int nextSlot = 0;
    std::vector<Log::stringLog> acceptorGroupCommittedLogs = {};
    std::unordered_map<std::string, Log::pValueLog> acceptorGroupUncommittedLogs = {}; //key = acceptor group ID

    two_p_set acceptorGroupIdSet;
    std::vector<std::string> acceptorGroupIds = {};
    int nextAcceptorGroup = 0;

    void listenToAnna(const std::string& key, const two_p_set& twoPSet);
    void listenToBatcher(const Batch& payload);
    void listenToProxyLeader(const ProxyLeaderToProposer& payload);
    void listenToProposer(const Ballot& leaderBallot);

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
