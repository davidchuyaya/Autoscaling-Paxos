//
// Created by Taj Shaik on 11/22/20.
//

#ifndef AUTOSCALING_PAXOS_SCALING_HPP
#define AUTOSCALING_PAXOS_SCALING_HPP

#include <string>
#include <sstream>
#include <cstdlib>
#include <vector>
#include <cstdio>
#include "../utils/config.hpp"

namespace scaling {
	std::vector<std::string> startBatchers(int numBatchers);
	/**
	 * @warning Only call this method once per execution, or else proposers will spawn with the same ID.
	 * @param numAcceptorGroups
	 */
	std::vector<std::string> startProposers(int numAcceptorGroups);
	std::vector<std::string> startProxyLeaders(int numProxyLeaders);
	std::vector<std::string> startAcceptorGroup(const std::string& acceptorGroupId);
	std::vector<std::string> startUnbatchers(int numUnbatchers);
    std::vector<std::string> startInstance(const std::string& executable, const std::string& arguments,
										   const std::string& name, int num);
	void killInstance(const std::string& name);
    void shutdown();
	std::vector<std::string> executeAndOutputToVector(const std::string& command);
};

#endif //AUTOSCALING_PAXOS_SCALING_HPP
