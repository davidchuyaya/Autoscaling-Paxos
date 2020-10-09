//
// Created by David Chu on 10/6/20.
//

#ifndef AUTOSCALING_PAXOS_LOG_HPP
#define AUTOSCALING_PAXOS_LOG_HPP

#include <vector>
#include <string>
#include <mutex>
#include <message.pb.h>

class Log {
public:
    std::mutex logMutex;
    std::vector<PValue> pValues = {};
};


#endif //AUTOSCALING_PAXOS_LOG_HPP
