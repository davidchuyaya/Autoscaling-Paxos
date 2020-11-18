//
// Created by Taj Shaik on 11/5/20.
//

#ifndef AUTOSCALING_PAXOS_ANNA_HPP
#define AUTOSCALING_PAXOS_ANNA_HPP

#include <string>
#include "storage.hpp"
#include "two_p_set.hpp"


class anna : public Storage {
public:
    void initClient();
    std::string getRequest(std::string key);
    void putRequest(std::string key, std::string value);
    two_p_set<std::string> get2Pset(const std::string& key);
    void put2Pset(const std::string& key, const two_p_set<std::string>& twoPSet);

private:
    inline static const std::string KEY_OBSERVED_PREFIX = "observed";
    inline static const std::string KEY_REMOVED_PREFIX = "removed";

    template<typename T>
    SetLattice<T> getLattice(const std::string& key) {
        //TODO anna stuff
    }
    template<typename T>
    void putLattice(const std::string& key, const SetLattice<T>& lattice) {
        //TODO anna stuff
    }
};

#endif //AUTOSCALING_PAXOS_ANNA_HPP
