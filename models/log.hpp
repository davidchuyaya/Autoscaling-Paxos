//
// Created by David Chu on 10/6/20.
//

#ifndef AUTOSCALING_PAXOS_LOG_HPP
#define AUTOSCALING_PAXOS_LOG_HPP

#include <vector>
#include <string>
#include <message.pb.h>

namespace Log {
    std::tuple<std::vector<std::string>, std::unordered_map<int, std::string>>
    committedAndUncommittedLog(const std::vector<std::vector<PValue>>& acceptorLogs);
    void printLog(const std::vector<PValue>& log);
    bool isBallotGreaterThan(const Ballot& ballotLeft, const Ballot& ballotRight);
}


#endif //AUTOSCALING_PAXOS_LOG_HPP
