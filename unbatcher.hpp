//
// Created by David Chu on 11/11/20.
//

#ifndef AUTOSCALING_PAXOS_UNBATCHER_HPP
#define AUTOSCALING_PAXOS_UNBATCHER_HPP

#include <shared_mutex>
#include <string>
#include <vector>
#include <thread>
#include <unordered_map>
#include <message.pb.h>
#include "utils/network.hpp"
#include "utils/heartbeater.hpp"
#include "lib/storage/anna_write_only.hpp"

class unbatcher {
public:
    explicit unbatcher(int id);
private:
    const int id;
	anna_write_only* annaWriteOnlyClient;

    std::shared_mutex ipToSocketMutex;
    std::unordered_map<std::string, int> ipToSocket = {};

    std::shared_mutex proxyLeaderMutex;
    std::vector<int> proxyLeaders;

    [[noreturn]] void startServer();
    int connectToClient(const std::string& ipAddress);

};


#endif //AUTOSCALING_PAXOS_UNBATCHER_HPP
