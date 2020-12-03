//
// Created by Taj Shaik on 11/22/20.
//

#ifndef AUTOSCALING_PAXOS_SCALING_HPP
#define AUTOSCALING_PAXOS_SCALING_HPP

#include <string>
#include <sstream>
#include <cstdlib>
#include "../utils/config.hpp"

namespace scaling {
	void startBatchers(int numBatchers);
	/**
	 * @warning Only call this method once per execution, or else proposers will spawn with the same ID.
	 * @param numAcceptorGroups
	 */
	void startProposers(int numAcceptorGroups);
	void startProxyLeaders(int numProxyLeaders);
	void startAcceptorGroup(const std::string& acceptorGroupId);
	void startUnbatchers(int numUnbatchers);
    void startInstance(const std::string& executable, const std::string& arguments, const std::string& name, int num);
    void shutdown();
};

#endif //AUTOSCALING_PAXOS_SCALING_HPP
