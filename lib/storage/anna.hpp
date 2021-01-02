//
// Created by Taj Shaik on 11/5/20.
//

#ifndef AUTOSCALING_PAXOS_ANNA_HPP
#define AUTOSCALING_PAXOS_ANNA_HPP

#include <shared_mutex>
#include <unordered_set>
#include <string>
#include <functional>
#include <optional>
#include <thread>
#include "client/kvs_client.hpp"
#include "two_p_set.hpp"
#include "../../utils/config.hpp"

using annaListener = std::function<void(const std::string&, const two_p_set&)>;

class anna {
public:
	static anna* writeOnly(const std::unordered_map<std::string, std::string>& keyValues);
	static anna* readWritable(const std::unordered_map<std::string, std::string>& keyValues, const annaListener& listener);
	void putSingletonSet(const std::string& key, const std::string& value);
	void removeSingletonSet(const std::string& key, const std::string& value);
    void subscribeTo(const std::string& key);
    void unsubscribeFrom(const std::string& key);

private:
	const bool isWriteOnly;
    std::shared_mutex clientMutex;
    KvsClient client;

    std::shared_mutex keysToListenToMutex;
    std::unordered_set<std::string> keysToListenTo = {};

    std::shared_mutex pendingPutsMutex;
    std::unordered_set<std::string> pendingPuts = {};

	anna(bool writeOnly, const std::unordered_map<std::string, std::string>& keyValues,
	  const annaListener& listener);
    void listenerThread(const annaListener& listener);
    void putLattice(const std::string& prefixedKey, const std::unordered_set<std::string>& lattice);
};

#endif //AUTOSCALING_PAXOS_ANNA_HPP
