//
// Created by David Chu on 11/16/20.
//

#include "two_p_set.hpp"

#include <utility>

two_p_set::two_p_set() : observed(), removed() {}
two_p_set::two_p_set(std::unordered_set<std::string> observed, std::unordered_set<std::string>  removed) :
        observed(std::move(observed)), removed(std::move(removed)) {}

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

std::string two_p_set::mergeAndUnprefixKey(std::string key, const std::unordered_set<std::string>& set) {
    auto observedPrefixPos = key.find(config::KEY_OBSERVED_PREFIX);
    if (observedPrefixPos != std::string::npos) {
        observed.insert(set.begin(), set.end());
        return key.erase(observedPrefixPos, config::KEY_OBSERVED_PREFIX.size());
    }
    else {
        auto removedPrefixPos = key.find(config::KEY_REMOVED_PREFIX);
        removed.insert(set.begin(), set.end());
        return key.erase(removedPrefixPos, config::KEY_REMOVED_PREFIX.size());
    }
}

two_p_set two_p_set::updatesFrom(const two_p_set& other) const {
    std::unordered_set<std::string> outputObserved;
    std::unordered_set<std::string> outputRemoved;
	for (const std::string& s : other.observed)
		if (observed.find(s) == observed.end())
			outputObserved.insert(s);
	for (const std::string& s : other.removed)
		if (removed.find(s) == removed.end())
			outputRemoved.insert(s);
    return {outputObserved, outputRemoved};
}

const std::unordered_set<std::string>& two_p_set::getObserved() const {
    return observed;
}

const std::unordered_set<std::string>& two_p_set::getRemoved() const {
    return removed;
}

bool two_p_set::empty() const {
    if (removed.size() < observed.size())
        return false;
    return std::all_of(observed.begin(), observed.end(), [&](const std::string& elem){
        return removed.find(elem) != removed.end(); //removed.contains(elem)
    });
}

std::string two_p_set::printSet() const {
	std::stringstream out;
	out << "Observed: {";
	for (const std::string& s : observed)
		out << s << ", ";
	out << "}. Removed: {";
	for (const std::string& s : removed)
		out << s << ", ";
	out << "}";
	return out.str();
}