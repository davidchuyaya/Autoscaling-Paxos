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
    const static int NUM_ACCEPTOR_GROUPS = 2;
    const static int THRESHOLD_BATCH_SIZE = 2;
    const static int MAIN_PORT = 10000;
    const static int PROPOSER_PORT_START = 11000;
    const static int ACCEPTOR_PORT_START = 12000;
    const static int ACCEPTOR_GROUP_PORT_OFFSET = 100;

    const static inline std::string LOCALHOST = "127.0.0.1";
    const static int TCP_READ_BUFFER_SIZE = 1024;
    const static int HEARTBEAT_TIMEOUT_SEC = 20; // this - HEARTBEAT_SLEEP_SEC = time allowed between message send & receive
    const static int HEARTBEAT_SLEEP_SEC = 5;
    const static int ID_SCOUT_DELAY_MULTIPLIER = 5; // this * proposer ID = number of seconds to delay before sending scouts
}

#endif //AUTOSCALING_PAXOS_CONFIG_HPP
