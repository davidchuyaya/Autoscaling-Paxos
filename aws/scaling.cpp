//
// Created by David Chu on 12/2/20.
//

#include "scaling.hpp"

void scaling::startBatchers(const int numBatchers) {
	startInstance("batcher", "", "batchers-?", numBatchers);
}

void scaling::startProposers(const int numAcceptorGroups) {
	for (int i = 1; i <= config::F+1; i++)
		startInstance("proposer", std::to_string(i) + " " + std::to_string(numAcceptorGroups),
				"proposer-" + std::to_string(i), 1);
}

void scaling::startProxyLeaders(const int numProxyLeaders) {
	startInstance("proxy_leader", "", "proxyLeader-?", numProxyLeaders);
}

void scaling::startAcceptorGroup(const std::string& acceptorGroupId) {
	startInstance("acceptor", acceptorGroupId, "acceptor-?." + acceptorGroupId, 2*config::F+1);
}

void scaling::startUnbatchers(const int numUnbatchers) {
	startInstance("unbatcher", "", "unbatcher-?", numUnbatchers);
}

void scaling::startInstance(const std::string& executable, const std::string& arguments,
							const std::string& name, const int num) {
	std::stringstream userData;
	userData << "'"
			 << "#!/bin/bash -xe\n"
			 << "mkdir /paxos\n"
	         << "cd /paxos\n"
	         << "wget https://autoscaling-paxos.s3-us-west-1.amazonaws.com/" << executable << "\n"
	         << "chmod +x " << executable << "\n"
	         << "export " << config::ENV_ANNA_ROUTING_NAME << "=" << config::ANNA_ROUTING_ADDRESS << "\n"
	         << "export " << config::ENV_IP_NAME << "=" << config::IP_ADDRESS << "\n"
	         << "export " << config::ENV_ANNA_KEY_PREFIX_NAME << "=" << config::ANNA_KEY_PREFIX << "\n"
	         << "./" << executable << " " << arguments
	         << "'";

	//since we're not using newlines here, remember to have spaces between things
	std::stringstream ec2Script;
	ec2Script << "aws ec2 run-instances "
	          << "--image-id ami-08ffb106d09e20436 "
	          << "--count " << num << " "
	          << "--instance-type c5.large "
	          << "--key-name anna "
	          << "--security-group-ids sg-0196a7a839c79446d "
			  << "--tag-specifications 'ResourceType=instance,Tags=[{Key=Name,Value=" << name << "}]' "
	          << "--user-data " << userData.str();

	system(ec2Script.str().c_str());
}