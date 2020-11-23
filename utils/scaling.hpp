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

namespace scaling {
    void static startInstance(const std::string& command, const std::string& instanceName, const std::string& instanceType)
    {
        std::stringstream ss;
        ss << config::AWS_USER_DATA_SCRIPT << instanceType << "\n" << config::AWS_MAKE_EXEC << instanceType << "\n" << config::AWS_IP_ENV << config::AWS_ANNA_ROUTING_ENV << command;
        std::string userDataScript = ss.str();
        const auto userDataBase64 = Aws::Utils::Array((unsigned char*) userDataScript.c_str(), userDataScript.size());

        Aws::EC2::EC2Client ec2;
        Aws::Utils::Base64::Base64 base64;

        Aws::EC2::Model::Tag name_tag;
        name_tag.SetKey("Name");
        name_tag.SetValue(Aws::String(instanceName.c_str(), instanceName.size()));

        Aws::EC2::Model::TagSpecification tagSpecification;
        tagSpecification.AddTags(name_tag);
        tagSpecification.SetResourceType(Aws::EC2::Model::ResourceType::instance);

        Aws::Vector<Aws::EC2::Model::TagSpecification> tagSpecifications(1);
        tagSpecifications.push_back(tagSpecification);

        Aws::EC2::Model::IamInstanceProfileSpecification iamProfile;
        iamProfile.SetArn(Aws::String(config::AWS_ARN_ID.c_str(), config::AWS_ARN_ID.size()));

        Aws::EC2::Model::RunInstancesRequest run_request;
        run_request.SetImageId(Aws::String(config::AWS_AMI_ID.c_str(), config::AWS_AMI_ID.size()));
        run_request.SetInstanceType(Aws::EC2::Model::InstanceType::t2_micro);
        run_request.SetMinCount(1);
        run_request.SetMaxCount(1);
        run_request.SetTagSpecifications(tagSpecifications);
        run_request.SetIamInstanceProfile(iamProfile);
        run_request.SetKeyName(Aws::String(config::AWS_PEM_FILE.c_str(), config::AWS_PEM_FILE.size()));
        run_request.SetUserData(base64.Encode(userDataBase64));

        auto run_outcome = ec2.RunInstances(run_request);
        if (!run_outcome.IsSuccess())
        {
            printf("Error for %s based on AMI %s : %s\n", instanceName.c_str(), config::AWS_AMI_ID.c_str(), run_outcome.GetError().GetMessage().c_str());
            return;
        }

        const auto& instances = run_outcome.GetResult().GetInstances();
        if (instances.size() == 0)
        {
            printf("Failed to start ec2 instance %s based on AMI: %s\n", instanceName.c_str(), config::AWS_AMI_ID.c_str());
            return;
        }
    }
};

#endif //AUTOSCALING_PAXOS_SCALING_HPP
