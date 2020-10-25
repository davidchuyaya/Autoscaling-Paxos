//
// Created by David Chu on 10/6/20.
//

#ifndef AUTOSCALING_PAXOS_LOG_HPP
#define AUTOSCALING_PAXOS_LOG_HPP

#include <vector>
#include <string>
#include <message.pb.h>

namespace Log {
    using stringLog = std::unordered_map<int, std::string>; //key = slot
    using pValueLog = std::unordered_map<int, PValue>;
    using acceptorGroupLog = std::vector<pValueLog>;
    using allAcceptorGroupLogs = std::unordered_map<int, acceptorGroupLog>; //key = acceptor group ID

    /**
     * Determines which values in logs of acceptors are committed (held by all acceptors) and which are not.
     *
     * @param acceptorGroupLogs List of logs received from acceptors
     * @invariant Size of acceptorLogs > F
     * @return Committed log, slot => payload; Uncommitted log, slot => payload; Acceptor group for slot, slot => acceptor group ID
     */
    std::tuple<stringLog, pValueLog, std::unordered_map<int, int>>
    committedAndUncommittedLog(const allAcceptorGroupLogs & acceptorGroupLogs);
    /**
     * Print the payloads in the log in order. Used for debugging.
     *
     * @param log Log to print
     */
    void printLog(const pValueLog& log);
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
