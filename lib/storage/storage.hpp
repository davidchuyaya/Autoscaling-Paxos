//
// Created by Taj Shaik on 11/5/20.
//

#ifndef AUTOSCALING_PAXOS_STORAGE_HPP
#define AUTOSCALING_PAXOS_STORAGE_HPP

#include <string>

class Storage {
public:
    virtual void initClient() = 0;
    virtual std::string getRequest(std::string key) = 0;
    virtual void putRequest(std::string key, std::string value) = 0;
};

#endif //AUTOSCALING_PAXOS_STORAGE_HPP
