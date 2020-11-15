//
// Created by David Chu on 10/6/20.
//

#ifndef AUTOSCALING_PAXOS_LOG_HPP
#define AUTOSCALING_PAXOS_LOG_HPP

#include <queue>
#include <vector>
#include <string>
#include <message.pb.h>
#include <sstream>
#include <tuple>
#include <unordered_map>
#include <queue>

namespace Log {
    using stringLog = std::unordered_map<int, std::string>; //key = slot
    using pValueLog = std::unordered_map<int, PValue>;
    using acceptorGroupLog = std::vector<pValueLog>;

    std::tuple<std::queue<int>, int>
    findHolesInLog(const stringLog& committedLog, const pValueLog& uncommittedLog);
    /**
     * Merge the committed logs of acceptor groups into the existing committed log.
     * A committed log only contains slots in which all acceptors within a group agree on the value.
     *
     * @note Invariant: No 2 committed logs have values for the same slots; no conflicting keys
     * @param committedLogs List of acceptor groups' committed logs
     */
    stringLog mergeCommittedLogs(const std::vector<stringLog>& committedLogs);
    /**
     * Merge the uncommitted logs of acceptor groups. If 2 acceptor groups have values for the same slot, the value with
     * the higher ballot is chosen. An uncommitted log only contains slots in which < F+1 acceptors agree upon the value.
     *
     * @param uncommittedLogs List of acceptor groups' uncommitted logs
     * @return Merged uncommitted logs
     */
    std::tuple<pValueLog, std::unordered_map<int, int>>
    mergeUncommittedLogs(const std::unordered_map<int, pValueLog>& uncommittedLogs);
    /**
     * Merge the logs of individual acceptors within the same acceptor group. The value at each slot is committed if all
     * acceptors hold that value; otherwise it is uncommitted and the value with the largest ballot is preserved.
     *
     * @param logs List of acceptors' logs
     * @return {committed log, uncommitted log}
     */
    std::tuple<stringLog, pValueLog>
    mergeLogsOfAcceptorGroup(const acceptorGroupLog& logs);
    /**
     * Returns string version of log for pretty printing. Used for debugging.
     *
     * @param log Log to print
     * @return String representation of log
     */
    std::string printLog(const pValueLog& log);
    /**
     * Compare ballots by partial order of ballot num, then ID.
     *
     * @param ballotLeft
     * @param ballotRight
     * @return True if ballotLeft > ballotRight
     */
    bool isBallotGreaterThan(const Ballot& ballotLeft, const Ballot& ballotRight);
}


#endif //AUTOSCALING_PAXOS_LOG_HPP
