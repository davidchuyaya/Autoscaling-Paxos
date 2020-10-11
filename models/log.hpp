//
// Created by David Chu on 10/6/20.
//

#ifndef AUTOSCALING_PAXOS_LOG_HPP
#define AUTOSCALING_PAXOS_LOG_HPP

#include <vector>
#include <string>
#include <message.pb.h>

namespace Log {
    /**
     * Determines which values in logs of acceptors are committed (held by all acceptors) and which are not.
     *
     * @param acceptorLogs List of logs received from acceptors
     * @invariant Size of acceptorLogs > F
     * @return Committed log, slot => payload; Uncommitted log, slot => payload
     */
    std::tuple<std::vector<std::string>, std::unordered_map<int, std::string>>
    committedAndUncommittedLog(const std::vector<std::vector<PValue>>& acceptorLogs);
    /**
     * Print the payloads in the log in order. Used for debugging.
     *
     * @param log Log to print
     */
    void printLog(const std::vector<PValue>& log);
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
