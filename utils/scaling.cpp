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
	std::stringstream ss;
	ss << config::AWS_USER_DATA_SCRIPT << executable << "\n"
		<< config::AWS_MAKE_EXEC << executable << "\n"
		<< config::AWS_IP_ENV
		<< config::AWS_ANNA_ROUTING_ENV
		<< "./" << executable << " " << arguments;
	const std::string& userDataScript = ss.str();
	const auto& userDataBase64 = Aws::Utils::Array((unsigned char*) userDataScript.c_str(), userDataScript.size());

	Aws::EC2::EC2Client ec2;
	Aws::Utils::Base64::Base64 base64;

	Aws::EC2::Model::Tag nameTag;
	nameTag.SetKey("Name");
	nameTag.SetValue(Aws::String(name.c_str(), name.size()));

	Aws::EC2::Model::TagSpecification tagSpecification;
	tagSpecification.AddTags(nameTag);
	tagSpecification.SetResourceType(Aws::EC2::Model::ResourceType::instance);

	Aws::Vector<Aws::EC2::Model::TagSpecification> tagSpecifications(1);
	tagSpecifications.push_back(tagSpecification);

	Aws::EC2::Model::IamInstanceProfileSpecification iamProfile;
	iamProfile.SetArn(Aws::String(config::AWS_ARN_ID.c_str(), config::AWS_ARN_ID.size()));

	Aws::EC2::Model::RunInstancesRequest runRequest;
	runRequest.SetImageId(Aws::String(config::AWS_AMI_ID.c_str(), config::AWS_AMI_ID.size()));
	runRequest.SetInstanceType(Aws::EC2::Model::InstanceType::c5_large);
	runRequest.SetMinCount(num);
	runRequest.SetMaxCount(num);
	runRequest.SetTagSpecifications(tagSpecifications);
	runRequest.SetIamInstanceProfile(iamProfile);
	runRequest.SetKeyName(Aws::String(config::AWS_PEM_FILE.c_str(), config::AWS_PEM_FILE.size()));
	runRequest.SetUserData(base64.Encode(userDataBase64));

	auto run_outcome = ec2.RunInstances(runRequest);
	if (!run_outcome.IsSuccess()) {
		LOG("Error for {} based on AMI {} : {}\n", name, config::AWS_AMI_ID, run_outcome.GetError().GetMessage());
		return;
	}

	const auto& instances = run_outcome.GetResult().GetInstances();
	if (instances.empty()) {
		LOG("Failed to start ec2 instance {} based on AMI: {}\n", name, config::AWS_AMI_ID);
		return;
	}
}