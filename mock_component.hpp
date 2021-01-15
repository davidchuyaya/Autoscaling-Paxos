//
// Created by David Chu on 1/11/21.
//

#ifndef AUTOSCALING_PAXOS_MOCK_COMPONENT_HPP
#define AUTOSCALING_PAXOS_MOCK_COMPONENT_HPP

#include <cstdio>
#include <cstdlib>
#include <string>
#include "mock/mock.hpp"
#include "utils/config.hpp"

class mock_component {
public:
	mock_component(int argc, const char** argv);
private:
	void printUsage(bool ifThisIsTrue = true);
};


#endif //AUTOSCALING_PAXOS_MOCK_COMPONENT_HPP
