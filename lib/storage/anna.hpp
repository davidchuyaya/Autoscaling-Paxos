//
// Created by Taj Shaik on 11/5/20.
//

#ifndef AUTOSCALING_PAXOS_ANNA_HPP
#define AUTOSCALING_PAXOS_ANNA_HPP

#include "storage.hpp"

class anna : public Storage {
    public:
        void initClient();
        std::string getRequest(std::string key);
        void putRequest(std::string key, std::string value);
};

#endif //AUTOSCALING_PAXOS_ANNA_HPP
