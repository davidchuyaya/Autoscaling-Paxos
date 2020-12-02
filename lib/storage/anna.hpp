//
// Created by Taj Shaik on 11/5/20.
//

#ifndef AUTOSCALING_PAXOS_ANNA_HPP
#define AUTOSCALING_PAXOS_ANNA_HPP

#include <shared_mutex>
#include <unordered_set>
#include <string>
#include <functional>
#include "client/kvs_client.hpp"
#include "two_p_set.hpp"
#include "utils/config.hpp"

using annaListener = std::function<void(const std::string&, const two_p_set&)>;

class anna {
public:
    anna(const std::unordered_set<std::string>& keysToListenTo, const annaListener& listener);
    anna(const std::string& key, const std::unordered_set<std::string>& keysToListenTo, const annaListener& listener);
	void putSingletonSet(const std::string& key, const std::string& value);
    void subscribeTo(const std::string& key);
    void unsubscribeFrom(const std::string& key);

private:
    std::shared_mutex clientMutex;
    KvsClient client;

    std::shared_mutex keysToListenToMutex;
    std::unordered_set<std::string> keysToListenTo = {};

    std::shared_mutex requestedKeysMutex;
    std::unordered_set<std::string> requestedKeys = {};

    [[noreturn]]
    void listenerThread(const annaListener& listener);
    [[noreturn]]
    void periodicGet2PSet();
    void putLattice(const std::string& prefixedKey, const std::unordered_set<std::string>& lattice);
};

#endif //AUTOSCALING_PAXOS_ANNA_HPP
