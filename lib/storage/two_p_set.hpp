//
// Created by David Chu on 11/15/20.
//

#ifndef AUTOSCALING_PAXOS_TWO_P_SET_HPP
#define AUTOSCALING_PAXOS_TWO_P_SET_HPP

#include <string>
#include <unordered_set>
#include <algorithm>
#include <sstream>
#include "lattices/core_lattices.hpp"
#include "../../utils/config.hpp"

class two_p_set {
public:
    two_p_set();
    two_p_set(std::unordered_set<std::string>&& observed, std::unordered_set<std::string>&& removed);
    two_p_set(const SetLattice<std::string>& observed, const SetLattice<std::string>& removed);

    void add(const std::string& s);
    void remove(const std::string& s);
    void merge(const two_p_set& other);
    std::string mergeAndUnprefixKey(std::string key, const SetLattice<std::string>& set);
    [[nodiscard]] two_p_set updatesFrom(const two_p_set& other) const;
    [[nodiscard]] const std::unordered_set<std::string>& getObserved() const;
    [[nodiscard]] const std::unordered_set<std::string>& getRemoved() const;
    [[nodiscard]] bool empty() const;
	[[nodiscard]] std::string printSet() const;
private:
    std::unordered_set<std::string> observed;
    std::unordered_set<std::string> removed;
};


#endif //AUTOSCALING_PAXOS_TWO_P_SET_HPP
