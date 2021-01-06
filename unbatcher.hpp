//
// Created by David Chu on 11/11/20.
//

#ifndef AUTOSCALING_PAXOS_UNBATCHER_HPP
#define AUTOSCALING_PAXOS_UNBATCHER_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <message.pb.h>
#include "utils/network.hpp"
#include "models/client_component.hpp"
#include "models/server_component.hpp"
#include "lib/storage/anna.hpp"

class unbatcher {
public:
    explicit unbatcher();
private:
	anna* annaClient;
	network* zmqNetwork;
	client_component* clients;
	server_component* proxyLeaders;

    void listenToProxyLeaders(const Batch& batch);
};


#endif //AUTOSCALING_PAXOS_UNBATCHER_HPP
