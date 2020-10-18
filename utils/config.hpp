//
// Created by David Chu on 10/5/20.
//

#ifndef AUTOSCALING_PAXOS_CONFIG_HPP
#define AUTOSCALING_PAXOS_CONFIG_HPP

/**
 * Designed to be swapped out with a configuration file or DB
 */
#include <array>
#include <numeric>

namespace config {
    const static int F = 1;
    const static int THRESHOLD_BATCH_SIZE = 2;
    const static int MAIN_PORT = 10000;
    const static int PROPOSER_PORT_START = 11000;
    const static int ACCEPTOR_PORT_START = 12000;

    const static inline std::string LOCALHOST = "127.0.0.1";
    const static int TCP_READ_BUFFER_SIZE = 1024;
    const static int LEADER_TIMEOUT_SEC = 5; // this - LEADER_HEARTBEAT_SLEEP_SEC = time allowed between message send & receive
    const static int LEADER_HEARTBEAT_SLEEP_SEC = 3;
}

#endif //AUTOSCALING_PAXOS_CONFIG_HPP
