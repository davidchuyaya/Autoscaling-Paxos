//
// Created by Taj Shaik on 11/5/20.
//

#include "anna.hpp"

void anna::initClient() {
    return;
}
std::string anna::getRequest(std::string key) {
    return "";
}
void anna::putRequest(std::string key, std::string value) {
    return;
}

two_p_set<std::string> anna::get2Pset(const string& key) {
    return two_p_set {getLattice<std::string>(KEY_OBSERVED_PREFIX + key),
            getLattice<std::string>(KEY_REMOVED_PREFIX + key)};
}

void anna::put2Pset(const std::string& key, const two_p_set<std::string>& twoPSet) {
    putLattice(KEY_OBSERVED_PREFIX + key, twoPSet.getObserved());
    putLattice(KEY_REMOVED_PREFIX + key, twoPSet.getRemoved());
}