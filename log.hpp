//
// Created by David Chu on 10/6/20.
//

#ifndef AUTOSCALING_PAXOS_LOG_HPP
#define AUTOSCALING_PAXOS_LOG_HPP

#include <vector>
#include <string>

struct logEntry {
    int id;
    int ballotNum;
    std::string payload;
};

class Log {
    std::vector<logEntry> log = {};
};


#endif //AUTOSCALING_PAXOS_LOG_HPP
