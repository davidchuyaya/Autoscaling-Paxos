//
// Created by David Chu on 11/15/20.
//

#ifndef AUTOSCALING_PAXOS_TWO_P_SET_HPP
#define AUTOSCALING_PAXOS_TWO_P_SET_HPP

#include <unordered_set>
#include <algorithm>
#include <common/include/lattices/core_lattices.hpp>

enum partial_order {
    GREATER_THAN,
    LESS_THAN,
    NOT_COMPARABLE,
    EQUAL_TO
};

template<typename T>
class two_p_set {
public:
    two_p_set<T>() : observed(), removed() {}
    two_p_set<T>(const SetLattice<T>& observed, const SetLattice<T>& removed) : observed(observed), removed(removed) {}

    void add(const T& t) {
        observed.insert(t);
    }

    void remove(const T& t) {
        removed.insert(t);
    }

    void merge(const two_p_set<T>& other) {
        observed.merge(other.observed);
        removed.merge(other.removed);
    }

    partial_order compare(const two_p_set<T>& other) const {
        partial_order observedCompare = compareSets(observed.reveal(), other.observed.reveal());
        partial_order removedCompare = compareSets(removed.reveal(), other.removed.reveal());

        if (observedCompare == removedCompare)
            return observedCompare;
        if (observedCompare == EQUAL_TO)
            return removedCompare;
        if (removedCompare == EQUAL_TO)
            return observedCompare;
        return NOT_COMPARABLE;
    }

    std::unordered_set<T> toSet() const {
        std::unordered_set<T> output;
        const std::unordered_set<T>& observedSet = observed.reveal();
        const std::unordered_set<T>& removedSet = removed.reveal();
        std::set_difference(observedSet.begin(), observedSet.end(), removedSet.begin(), removedSet.end(),
                            std::inserter(output, output.end()));
        return output;
    }

    const SetLattice<T>& getObserved() const {
        return observed;
    }

    const SetLattice<T>& getRemoved() const {
        return removed;
    }
private:
    SetLattice<T> observed;
    SetLattice<T> removed;

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
