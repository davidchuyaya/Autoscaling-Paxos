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
#include "spdlog/spdlog.h"

#define INIT_LOGGER() spdlog::basic_logger_mt("paxos_log", "log.txt");spdlog::get("paxos_log")->flush_on(spdlog::level::info)
#define BENCHMARK_LOG(...) spdlog::get("paxos_log")->info(__VA_ARGS__) //some logging is always on for benchmarks

//#define DEBUG
#ifdef DEBUG
#   define LOG(...) BENCHMARK_LOG(__VA_ARGS__)
#   define TIME() LOG("Micro: {}\n", std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count())
#else //noop
#   define LOG(...) void(0)
#   define TIME() do{}while(0)
#endif

namespace config {
    const static int F = 1;
    const static int CLIENT_PORT = 10000;
    const static int PROPOSER_PORT = 11000;
    const static int ACCEPTOR_PORT = 12000;
    const static int BATCHER_PORT = 13000;
    const static int UNBATCHER_PORT = 14000;
    const static int SERVER_MAX_CONNECTIONS = 200;

    const static inline std::string ENV_ANNA_ROUTING_NAME = "ANNA_ROUTING";
    const static inline std::string ENV_IP_NAME = "IP";
    const static inline std::string ENV_ANNA_KEY_PREFIX_NAME = "ANNA_KEY_PREFIX";
	const static inline std::string ENV_BATCH_SIZE_NAME = "BATCH_SIZE";

    const static inline std::string IP_ADDRESS = std::getenv(ENV_IP_NAME.c_str());
    const static inline std::string ANNA_ROUTING_ADDRESS = std::getenv(ENV_ANNA_ROUTING_NAME.c_str());
    const static inline std::string ANNA_KEY_PREFIX = std::getenv(ENV_ANNA_KEY_PREFIX_NAME.c_str());
	const static int BATCH_SIZE = std::stoi(std::getenv(ENV_BATCH_SIZE_NAME.c_str()));

    const static int TCP_RETRY_TIMEOUT_SEC = 10;
    const static int HEARTBEAT_TIMEOUT_SEC = 20; // this - HEARTBEAT_SLEEP_SEC = time allowed between message send & receive
    const static int HEARTBEAT_SLEEP_SEC = 5;
    const static int BATCHER_TIMEOUT_SEC = 5;
    const static int CLIENT_TIMEOUT_SEC = 1;
    const static int ID_SCOUT_DELAY_MULTIPLIER = 5; // this * proposer ID = number of seconds to delay before sending scouts
    const static int ANNA_RECHECK_SEC = 3; // how often we send a new request to Anna & how often we check for updates

    const static inline std::string KEY_OBSERVED_PREFIX = ANNA_KEY_PREFIX + "observed";
    const static inline std::string KEY_REMOVED_PREFIX = ANNA_KEY_PREFIX + "removed";
    const static inline std::string KEY_BATCHERS = "Batchers";
    const static inline std::string KEY_PROPOSERS = "Proposers";
    const static inline std::string KEY_PROXY_LEADERS = "ProxyLeaders";
    const static inline std::string KEY_ACCEPTOR_GROUPS = "AcceptorGroups";
    const static inline std::string KEY_UNBATCHERS = "Unbatchers";

	const static inline std::string REQUEST_DELIMITER = "|";
}

#endif //AUTOSCALING_PAXOS_CONFIG_HPP
