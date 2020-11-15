//
// Created by David Chu on 11/15/20.
//

#ifndef AUTOSCALING_PAXOS_TEST_HPP
#define AUTOSCALING_PAXOS_TEST_HPP

#include "client/kvs_client.hpp"
#include "threads.hpp"

class test {
public:
    explicit test() {
        UserRoutingThread thread = {"127.0.0.1", 1};
        std::vector<UserRoutingThread> threads = {thread};
        KvsClient client = {threads, "127.0.0.1"};
    }
};


#endif //AUTOSCALING_PAXOS_TEST_HPP
