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

class anna {
public:
    explicit anna();
    anna(const std::unordered_set<std::string>& keysToListenTo,
         const std::function<void(const std::string&, const two_p_set&)>& listener);
    anna(const std::string& key, const std::unordered_set<std::string>& keysToListenTo,
         const std::function<void(const std::string&, const two_p_set&)>& listener);
    void put2Pset(const std::string& key, const two_p_set& twoPSet);
	void putSingletonSet(const std::string& key, const std::string& value);
    void subscribeTo(const std::string& key);
    void unsubscribeFrom(const std::string& key);

private:
    std::shared_mutex clientMutex;
    KvsClient client;

    std::shared_mutex keysToListenToMutex;
    std::unordered_set<std::string> keysToListenTo = {};

    [[noreturn]]
    void listenerThread(const std::function<void(const std::string&, const two_p_set&)>& listener); //TODO don't just listen to 2p-sets?
    [[noreturn]]
    void periodicGet2PSet();
    void putLattice(const std::string& key, const SetLattice<std::string>& lattice);
};

#endif //AUTOSCALING_PAXOS_ANNA_HPP
