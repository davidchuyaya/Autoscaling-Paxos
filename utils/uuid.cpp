//
// Created by David Chu on 10/31/20.
//

#include "uuid.hpp"

int uuid::generate() {
    std::lock_guard lock(randomNumMutex);
    return randomInt32(rng);
}