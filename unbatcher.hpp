//
// Created by David Chu on 11/11/20.
//

#ifndef AUTOSCALING_PAXOS_UNBATCHER_HPP
#define AUTOSCALING_PAXOS_UNBATCHER_HPP

#include <mutex>
#include <string>
#include <vector>
#include <thread>
#include <unordered_map>
#include <message.pb.h>

class unbatcher {
public:
    explicit unbatcher(int id);
private:
    const int id;
    std::mutex ipToSocketMutex;
    std::unordered_map<std::string, int> ipToSocket = {};

    std::mutex proxyLeaderMutex;
    std::vector<int> proxyLeaders;

    [[noreturn]] void startServer();
    int connectToClient(const std::string& ipAddress);

};


#endif //AUTOSCALING_PAXOS_UNBATCHER_HPP
