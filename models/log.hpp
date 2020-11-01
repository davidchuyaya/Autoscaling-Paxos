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

    stringLog mergeCommittedLogs(const std::vector<stringLog>& committedLogs);

    std::tuple<pValueLog, std::unordered_map<int, int>>
    mergeUncommittedLogs(const std::unordered_map<int, pValueLog>& uncommittedLogs);

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
