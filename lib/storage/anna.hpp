//
// Created by Taj Shaik on 11/5/20.
//

#ifndef AUTOSCALING_PAXOS_ANNA_HPP
#define AUTOSCALING_PAXOS_ANNA_HPP

#include <string>
#include "client/kvs_client.hpp"
#include "two_p_set.hpp"
#include "utils/config.hpp"

class anna {
public:
    explicit anna(const std::optional<std::function<void(two_p_set&)>>& listener);
    anna(const std::string& key, const std::string& value, const std::optional<std::function<void(two_p_set&)>>& listener);
    void periodicGet2PSet(const std::string& key);
    void put2Pset(const std::string& key, const two_p_set& twoPSet);

private:
    //TODO find out if we need to lock the client for async receive
    KvsClient client;

    [[noreturn]]
    void listenerThread(const std::function<void(two_p_set&)>& listener); //TODO don't just listen to 2p-sets?
    void putLattice(const std::string& key, const SetLattice<std::string>& lattice);
};

#endif //AUTOSCALING_PAXOS_ANNA_HPP
