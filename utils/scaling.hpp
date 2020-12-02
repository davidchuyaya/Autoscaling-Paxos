//
// Created by Taj Shaik on 11/22/20.
//

#ifndef AUTOSCALING_PAXOS_SCALING_HPP
#define AUTOSCALING_PAXOS_SCALING_HPP

#include <string>

#include <aws/core/Aws.h>
#include <aws/core/utils/base64/Base64.h>
#include <aws/core/utils/Array.h>
#include <aws/ec2/EC2Client.h>
#include <aws/ec2/model/UserData.h>
#include <aws/ec2/model/CreateTagsRequest.h>
#include <aws/ec2/model/RunInstancesRequest.h>
#include <aws/ec2/model/RunInstancesResponse.h>
#include <aws/ec2/model/TagSpecification.h>
#include "config.hpp"

namespace scaling {
	void startBatchers(int numBatchers);
	void startProposers(int numAcceptorGroups);
	void startProxyLeaders(int numProxyLeaders);
	void startAcceptorGroup(const std::string& acceptorGroupId);
	void startUnbatchers(int numUnbatchers);
    void startInstance(const std::string& executable, const std::string& arguments, const std::string& name, int num);
};

#endif //AUTOSCALING_PAXOS_SCALING_HPP
