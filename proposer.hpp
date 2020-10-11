//
// Created by David Chu on 10/4/20.
//

#ifndef C__PAXOS_PROPOSER_HPP
#define C__PAXOS_PROPOSER_HPP


#include <vector>
#include <deque>
#include "utils/network.hpp"
#include "message.pb.h"
#include "models/log.hpp"

class proposer {
public:
    explicit proposer(int id);
private:
    const int id; // 0 indexed, no gaps
    std::mutex ballotMutex;
    int ballotNum = 0;
    bool isLeader = false;

    bool shouldSendScouts = true;
    std::mutex scoutMutex;
    int numApprovedScouts = 0;
    int numPreemptedScouts = 0;

    std::mutex commanderMutex;
    std::unordered_map<int, int> slotToApprovedCommanders = {};
    std::unordered_map<int, int> slotToPreemptedCommanders = {};

    std::mutex unproposedPayloadsMutex;
    std::vector<std::string> unproposedPayloads = {};
    std::unordered_map<int, std::string> uncommittedProposals = {}; //invariant: empty until we are leader
    std::vector<std::string> log; //TODO don't store the entire log
    std::mutex acceptorLogsMutex;
    std::vector<std::vector<PValue>> acceptorLogs = {};

    std::mutex proposerMutex;
    std::vector<int> proposerSockets = {};
    std::mutex acceptorMutex;
    std::vector<int> acceptorSockets = {};
    std::vector<std::thread> threads = {}; // A place to put threads so they don't get freed

    /**
     * Load new messages into unproposedPayloads, newest last.
     */
    [[noreturn]] void listenToMain();

    [[noreturn]] void startServer();
    void connectToProposers();
    [[noreturn]] void listenToProposer(int socket);

    void connectToAcceptors();
    /**
     * Process p1b and p2b messages from acceptors.
     * @param socket Socket ID of acceptor
     */
    [[noreturn]] void listenToAcceptor(int socket);
    void broadcastToAcceptors(const google::protobuf::Message& message);

    /**
     * Execute all scout/commander logic.
     * @note Variables that are only used in the main loop do not need to be locked, as they are used in a single thread.
     */
    [[noreturn]] void mainLoop();
    /**
     * Broadcast p1a to acceptors to become the leader.
     * @invariant isLeader = false, shouldSendScouts = true
     */
    void sendScouts();
    /**
     * Check if more than F p1b's have been received. If so, determine if we are the new leader.
     * Compile logs received, removing committed items from our proposals and, if we are the new leader,
     * adding uncommitted items to our own proposal at the right slot.
     *
     * @invariant isLeader = false, shouldSendScouts = false, uncommittedProposals.empty()
     */
    void checkScouts();
    /**
     * Update log with newly committed slots from acceptors. Remove committed proposals from unproposedPayloads.
     * Propose uncommitted slots, add to uncommittedProposals
     * @invariant acceptorLogs contains F+1 acceptors' logs, uncommittedProposals.empty()
     */
    void mergeLogs();
    /**
     * Assign the next available slots to unproposedPayloads and send p2a messages for them.
     * @invariant isLeader = true
     */
    void sendCommandersForPayloads();
    /**
     * Send p2a messages for a given payload.
     * @param slot
     * @param payload
     */
    void sendCommanders(int slot, const std::string &payload);
    /**
     * Check if more than F p2b's have been received, once for each uncommitted slot. If yes, then confirm that slot
     * as committed. If we've been preempted, then that means another has become the leader. Move all uncommittedProposals
     * back into unproposedPayloads.
     * @invariant isLeader = true
     */
    void checkCommanders();
};


#endif //C__PAXOS_PROPOSER_HPP
