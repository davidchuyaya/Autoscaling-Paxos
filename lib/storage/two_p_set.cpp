//
// Created by David Chu on 11/16/20.
//

#include "two_p_set.hpp"

two_p_set::two_p_set() : observed(), removed() {}
two_p_set::two_p_set(std::unordered_set<std::string>&& observed, std::unordered_set<std::string>&& removed) :
        observed(observed), removed(removed) {}
two_p_set::two_p_set(const SetLattice<std::string>& observed, const SetLattice<std::string>& removed) :
        observed(observed.reveal()), removed(removed.reveal()) {}

void two_p_set::add(const std::string& s) {
    observed.insert(s);
}

void two_p_set::remove(const std::string& s) {
    removed.insert(s);
}

void two_p_set::merge(const two_p_set& other) {
    observed.insert(other.observed.begin(), other.observed.end());
    removed.insert(other.removed.begin(), other.removed.end());
}

void two_p_set::merge(const std::string& key, const SetLattice<std::string>& set) {
    const std::unordered_set<std::string>& revealed = set.reveal();
    if (isPrefix(config::KEY_OBSERVED_PREFIX, key))
        observed.insert(revealed.begin(), revealed.end());
    else if (isPrefix(config::KEY_REMOVED_PREFIX, key))
        removed.insert(revealed.begin(), revealed.end());
}

two_p_set two_p_set::updatesFrom(const two_p_set& other) {
    std::unordered_set<std::string> outputObserved;
    std::unordered_set<std::string> outputRemoved;
    std::set_difference(other.observed.begin(), other.observed.end(), observed.begin(), observed.end(),
                        std::inserter(outputObserved, outputObserved.end()));
    std::set_difference(other.removed.begin(), other.removed.end(), removed.begin(), removed.end(),
                        std::inserter(outputObserved, outputObserved.end()));
    return {outputObserved, outputRemoved};
}

partial_order two_p_set::compare(const two_p_set& other) const {
    partial_order observedCompare = compareSets(observed, other.observed);
    partial_order removedCompare = compareSets(removed, other.removed);

    if (observedCompare == removedCompare)
        return observedCompare;
    if (observedCompare == EQUAL_TO)
        return removedCompare;
    if (removedCompare == EQUAL_TO)
        return observedCompare;
    return NOT_COMPARABLE;
}

std::unordered_set<std::string> two_p_set::toSet() const {
    std::unordered_set<std::string> output;
    std::set_difference(observed.begin(), observed.end(), removed.begin(), removed.end(),
                        std::inserter(output, output.end()));
    return output;
}

const std::unordered_set<std::string>& two_p_set::getObserved() const {
    return observed;
}

const std::unordered_set<std::string>& two_p_set::getRemoved() const {
    return removed;
}

bool two_p_set::isPrefix(const std::string& prefix, const std::string& target) {
    //from https://stackoverflow.com/a/7913978/4028758
    return std::mismatch(prefix.begin(), prefix.end(), target.begin()).first == prefix.end();
}