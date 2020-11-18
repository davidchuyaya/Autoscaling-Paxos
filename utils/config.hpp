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
#define DEBUG

#ifdef DEBUG
#   define LOG(...) printf(__VA_ARGS__)
#else //noop
#   define LOG(...) do{}while(0)
#endif

namespace config {
    const static int F = 1;
    const static int NUM_ACCEPTOR_GROUPS = 2;
    const static int THRESHOLD_BATCH_SIZE = 2;
    const static int CLIENT_PORT = 10000;
    const static int PROPOSER_PORT_START = 11000;
    const static int ACCEPTOR_PORT_START = 12000;
    const static int ACCEPTOR_GROUP_PORT_OFFSET = 100;
    const static int BATCHER_PORT_START = 13000;
    const static int UNBATCHER_PORT_START = 14000;

    //TODO Store result from "curl http://169.254.169.254/latest/meta-data/public-ipv4" into env
    const static inline std::string IP_ADDRESS = std::getenv("IP");
    const static inline std::string ANNA_ROUTING_ADDRESS = std::getenv("ANNA_ROUTING");
    const static inline std::string ANNA_FUNCTION_ADDRESS = std::getenv("ANNA_FUNCTION");

    const static int BATCH_TIME_SEC = 5;
    const static int TCP_RETRY_TIMEOUT_SEC = 10;
    const static int HEARTBEAT_TIMEOUT_SEC = 20; // this - HEARTBEAT_SLEEP_SEC = time allowed between message send & receive
    const static int HEARTBEAT_SLEEP_SEC = 5;
    const static int ID_SCOUT_DELAY_MULTIPLIER = 5; // this * proposer ID = number of seconds to delay before sending scouts
    const static int ZMQ_RECEIVE_RETRY_SEC = 1;
    const static int ANNA_RECHECK_SEC = 10;

    const static inline std::string KEY_OBSERVED_PREFIX = "observed";
    const static inline std::string KEY_REMOVED_PREFIX = "removed";
    const static inline std::string KEY_BATCHERS = "Batchers";
    const static inline std::string KEY_PROPOSERS = "Proposers";
    const static inline std::string KEY_PROXY_LEADERS = "ProxyLeaders";
    const static inline std::string KEY_ACCEPTORS = "Acceptors";
    const static inline std::string KEY_UNBATCHERS = "Unbatchers";
}

#endif //AUTOSCALING_PAXOS_CONFIG_HPP
