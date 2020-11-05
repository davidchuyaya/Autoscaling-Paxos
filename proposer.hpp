//
// Created by David Chu on 10/4/20.
//

#ifndef C__PAXOS_PROPOSER_HPP
#define C__PAXOS_PROPOSER_HPP


#include <vector>
#include <deque>
#include "utils/network.hpp"
#include "utils/parser.hpp"
#include "message.pb.h"
#include "models/log.hpp"

class proposer {
public:
    explicit proposer(const int id, std::map<int, std::string> proposers, std::map<int, std::map<int, std::string>> acceptors);
private:
    const int id; // 0 indexed, no gaps

    std::mutex ballotMutex;
    int ballotNum = 0; // must be at least 1 the first time it is sent

    std::atomic<bool> isLeader = false;
    std::mutex heartbeatMutex;
    time_t lastLeaderHeartbeat;
    std::unordered_map<int, time_t> proxyLeaderHeartbeats = {}; //key = socket

    std::atomic<bool> shouldSendScouts = true;
    std::mutex remainingAcceptorGroupsForScoutsMutex;
    std::unordered_set<int> remainingAcceptorGroupsForScouts = {};

    std::mutex unproposedPayloadsMutex;
    std::vector<std::string> unproposedPayloads = {};

    std::mutex logMutex;
    Log::stringLog log;
    int lastCommittedSlot = 0;

    std::mutex uncommittedProposalsMutex;
    Log::stringLog uncommittedProposals = {}; //invariant: empty until we are leader. Key = slot

    std::mutex acceptorGroupLogsMutex;
    std::vector<Log::stringLog> acceptorGroupCommittedLogs = {};
    std::unordered_map<int, Log::pValueLog> acceptorGroupUncommittedLogs = {}; //key = acceptor group ID

    std::mutex proposerMutex;
    std::vector<int> proposerSockets = {};

    std::mutex acceptorMutex;
    std::vector<int> acceptorGroupIds = {};

    int nextAcceptorGroup = 0;

    std::mutex proxyLeaderMutex;
    std::condition_variable proxyLeaderCV;
    std::vector<int> fastProxyLeaders = {};
    std::vector<int> slowProxyLeaders = {};
    std::unordered_map<int, std::unordered_map<int, ProposerToAcceptor>> proxyLeaderSentMessages = {}; //{socket: {messageID: message}}

    int nextProxyLeader = 0;

    std::vector<std::thread> threads = {}; // A place to put threads so they don't get freed

    /**
     * Set acceptorGroupIds. TODO not hardcode the IDs
     */
    void findAcceptorGroupIds(std::map<int, std::map<int, std::string>> acceptors);

    /**
     * If isLeader = true, periodically tell other proposers.
     */
    [[noreturn]] void broadcastIAmLeader();
    /**
     * 1. Check if a leader has sent us a heartbeat. If it timed out, prepare to send scouts.
     * 2. Check if proxy leaders have sent us a heartbeat. If they timed out, remove it and send its messages to different proxy leaders.
     */
    [[noreturn]] void checkHeartbeats();

    [[noreturn]] void startServer();
    void listenToBatcher(int socket);
    void listenToProxyLeader(int socket);
    void connectToProposers(std::map<int, std::string> proposers);
    void listenToProposer(int socket);

    /**
     * Execute all scout/commander logic.
     */
    [[noreturn]] void mainLoop();
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
     * Assign the next available slots to unproposedPayloads and send p2a messages for them.
     * @invariant isLeader = true
     */
    void sendCommandersForPayloads();
    /**
     * Send p2a messages for a given payload.
     * Stores this event by calling sendToProxyLeader().
     *
     * @warning Does NOT lock proxyLeaderMutex or ballotMutex. The caller MUST lock both.
     * @param acceptorGroupId
     * @param slot
     * @param payload
     */
    void sendCommanders(int acceptorGroupId, int slot, const std::string& payload);
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
    int fetchNextAcceptorGroupId();
    /**
     * Increments (round robin) the next proxy leader a payload will be sent to.
     * @warning Does NOT lock proxyLeaderMutex. The caller MUST lock it.
     * @return The socket of the proxy leader to send to.
     */
    int fetchNextProxyLeaderSocket();
    /**
     * Stores the fact that we've sent this message to this proxy leader so we can resend if the proxy leader fails.
     *
     * @warning Does NOT lock proxyLeaderMutex. The caller MUST lock it.
     * @param proxyLeaderSocket
     * @param message
     */
    void sendToProxyLeader(int proxyLeaderSocket, const ProposerToAcceptor& message);

    /**
     * Find the newest slot in which all previous slots have been committed.
     * @warning Does NOT lock logMutex. The caller MUST lock it
     */
    void calcLastCommittedSlot();
};


#endif //C__PAXOS_PROPOSER_HPP
