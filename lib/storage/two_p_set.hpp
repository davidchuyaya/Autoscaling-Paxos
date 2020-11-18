//
// Created by David Chu on 11/15/20.
//

#ifndef AUTOSCALING_PAXOS_TWO_P_SET_HPP
#define AUTOSCALING_PAXOS_TWO_P_SET_HPP

#include <string>
#include <unordered_set>
#include <algorithm>
#include "lattices/core_lattices.hpp"
#include "utils/config.hpp"

enum partial_order {
    GREATER_THAN,
    LESS_THAN,
    NOT_COMPARABLE,
    EQUAL_TO
};

class two_p_set {
public:
    two_p_set();
    two_p_set(std::unordered_set<std::string>&& observed, std::unordered_set<std::string>&& removed);
    two_p_set(const SetLattice<std::string>& observed, const SetLattice<std::string>& removed);

    void add(const std::string& s);
    void remove(const std::string& s);
    void merge(const two_p_set& other);
    void merge(const std::string& key, const SetLattice<std::string>& set);
    two_p_set updatesFrom(const two_p_set& other);
    partial_order compare(const two_p_set& other) const;
    std::unordered_set<std::string> toSet() const;
    const std::unordered_set<std::string>& getObserved() const;
    const std::unordered_set<std::string>& getRemoved() const;
private:
    std::unordered_set<std::string> observed;
    std::unordered_set<std::string> removed;

    bool isPrefix(const std::string& prefix, const std::string& target);
    template<typename T>
    partial_order compareSets(const std::unordered_set<T>& s1, const std::unordered_set<T>& s2) const {
        bool s1ContainsAllInS2 = true;
        int matches = 0;
        for (const T& t2 : s2) {
            if (s1.find(t2) == s1.end())
                s1ContainsAllInS2 = false;
            else
                matches += 1;
        }

        if (s1ContainsAllInS2) {
            if (s1.size() == s2.size())
                return EQUAL_TO;
            else //s1 must be larger
                return GREATER_THAN;
        }
        else {
            if (matches == s1.size()) //iterated through all of s1, but s2 contains things s1 does not
                return LESS_THAN;
            else //both contain what the other does not
                return NOT_COMPARABLE;
        }
    }
};


#endif //AUTOSCALING_PAXOS_TWO_P_SET_HPP
