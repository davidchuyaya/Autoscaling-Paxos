//
// Created by David Chu on 12/2/20.
//

#include "scaling.hpp"

std::vector<std::string> scaling::startBatchers(const int numBatchers) {
	return startInstance("batcher", "", "batchers-?", numBatchers);
}

std::vector<std::string> scaling::startProposers(const int numAcceptorGroups) {
	const std::string& numAcceptorGroupsString = std::to_string(numAcceptorGroups);
	std::vector<std::string> instances = {};
	for (int i = 1; i <= config::F+1; i++) {
		const std::vector<std::string>& instanceId = startInstance("proposer",std::to_string(i)
			+ " " + numAcceptorGroupsString,"proposer-" + std::to_string(i), 1);
		instances.insert(instances.end(), instanceId.begin(), instanceId.end());
	}
	return instances;
}

std::vector<std::string> scaling::startProxyLeaders(const int numProxyLeaders) {
	return startInstance("proxy_leader", "", "proxyLeader-?", numProxyLeaders);
}

std::vector<std::string> scaling::startAcceptorGroup(const std::string& acceptorGroupId) {
	return startInstance("acceptor", acceptorGroupId, "acceptor-?." + acceptorGroupId, 2*config::F+1);
}

std::vector<std::string> scaling::startUnbatchers(const int numUnbatchers) {
	return startInstance("unbatcher", "", "unbatcher-?", numUnbatchers);
}

std::vector<std::string> scaling::startInstance(const std::string& executable, const std::string& arguments,
							const std::string& name, const int num) {
	std::stringstream userData;
	userData << "'"
			 << "#!/bin/bash -xe\n"
			 << "mkdir /paxos\n"
	         << "cd /paxos\n"
	         << "wget https://" << config::AWS_S3_BUCKET << ".s3-" << config::AWS_REGION << ".amazonaws.com/" << executable << "\n"
	         << "chmod +x " << executable << "\n"
	         << "export " << config::ENV_ANNA_ROUTING_NAME << "=" << config::ANNA_ROUTING_ADDRESS << "\n"
	         << "export " << config::ENV_IP_NAME << "=$(curl http://169.254.169.254/latest/meta-data/public-ipv4)\n"
	         << "export " << config::ENV_ANNA_KEY_PREFIX_NAME << "=" << config::ANNA_KEY_PREFIX << "\n"
	         << "export " << config::ENV_BATCH_SIZE_NAME << "=" << config::BATCH_SIZE << "\n"
			 << "export " << config::ENV_AWS_REGION_NAME << "=" << config::AWS_REGION << "\n"
			 << "export " << config::ENV_AWS_AMI_NAME << "=" << config::AWS_AMI << "\n"
			 << "export " << config::ENV_AWS_S3_BUCKET_NAME << "=" << config::AWS_S3_BUCKET << "\n"
			 << "./" << executable << " " << arguments
	         << "'";

	//since we're not using newlines here, remember to have spaces between things
	std::stringstream ec2Script;
	ec2Script << "aws ec2 run-instances "
	          << "--image-id " << config::AWS_AMI << " "
	          << "--count " << num << " "
	          << "--instance-type m5.2xlarge "
	          << "--key-name paxos-key "
	          << "--security-groups paxos-security-group "
			  << "--tag-specifications 'ResourceType=instance,Tags=[{Key=Name,Value=" << config::ANNA_KEY_PREFIX << "_"
			        << name << "}]' "
			  << "--query 'Instances[*].InstanceId' "
			  << "--output text "
	          << "--user-data " << userData.str();

	return executeAndOutputToVector(ec2Script.str());
}

void scaling::shutdown() {
	system("shutdown -h now");
}

void scaling::killInstance(const std::string& instanceId) {
	//since we're not using newlines here, remember to have spaces between things
	std::stringstream terminateScript;
	terminateScript << "aws ec2 terminate-instances "
	          << "--instance-ids " << instanceId;

	system(terminateScript.str().c_str());
}

std::vector<std::string> scaling::executeAndOutputToVector(const std::string& command) {
	FILE* file = popen(command.c_str(), "r");

	std::stringstream output;
	char readBuffer[128];
	while (fgets(readBuffer, 128, file))
		output << readBuffer;

	std::vector<std::string> outputVector;
	std::istringstream splitter(output.str());
	std::string temp;
	while (splitter >> temp)
		outputVector.emplace_back(temp);

	return outputVector;
}