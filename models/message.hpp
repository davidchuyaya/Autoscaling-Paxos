//
// Created by David Chu on 10/6/20.
//

#ifndef AUTOSCALING_PAXOS_MESSAGE_HPP
#define AUTOSCALING_PAXOS_MESSAGE_HPP

#include <unordered_set>
#include <string>
#include "anna.pb.h"
#include "message.pb.h"
#include "log.hpp"
#include "../utils/uuid.hpp"
#include "../utils/config.hpp"

namespace message {
    ProposerToAcceptor createP1A(int id, int ballotNum, const std::string& acceptorGroupId);
    AcceptorToProxyLeader createP1B(int messageId, const std::string& acceptorGroupId, const Ballot& highestBallot,
                                    const Log::pValueLog& log);
    ProposerToAcceptor createP2A(int id, int ballotNum, int slot, const std::string& client, const std::string& payload,
                                 const std::string& acceptorGroupId);
    AcceptorToProxyLeader createP2B(int messageId, const std::string& acceptorGroupId, const Ballot& highestBallot, int slot);
    ProxyLeaderToProposer createProxyP1B(int messageId, const std::string& acceptorGroupId, const Ballot& highestBallot,
                                         const Log::stringLog& committedLog, const Log::pValueLog& uncommittedLog);
    ProxyLeaderToProposer createProxyP2B(int messageId, const std::string& acceptorGroupId, const Ballot& highestBallot,
                                         int slot);
    Batch createBatchMessage(const std::string& ipAddress, const std::string& requests);

	KeyRequest createAnnaPutRequest(const std::string& prefixedKey, const std::string& payload);
	KeyRequest createAnnaGetRequest(const std::string& prefixedKey);
	KeyAddressRequest createAnnaKeyAddressRequest(const std::string& prefixedKey);
	SetValue createAnnaSet(const std::unordered_set<std::string>& set);
}

#endif //AUTOSCALING_PAXOS_MESSAGE_HPP
