//
// Created by David Chu on 10/31/20.
//

#ifndef AUTOSCALING_PAXOS_UUID_HPP
#define AUTOSCALING_PAXOS_UUID_HPP


#include <random>

namespace uuid {
    // generate random message IDs
    static std::mutex randomNumMutex;
    static std::random_device rd;
    static std::default_random_engine rng(rd());
    static std::uniform_int_distribution<int32_t> randomInt32(INT32_MIN, INT32_MAX);

    /**
     * Generate a random 32-bit int. Is thread-safe.
     * @return The random int.
     */
    int generate();
};


#endif //AUTOSCALING_PAXOS_UUID_HPP
