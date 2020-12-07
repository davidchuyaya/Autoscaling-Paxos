//
// Created by David Chu on 10/6/20.
//

#ifndef AUTOSCALING_PAXOS_MESSAGE_HPP
#define AUTOSCALING_PAXOS_MESSAGE_HPP

#include <string>
#include <random>
#include <vector>
#include <unordered_map>
#include "message.pb.h"
#include "log.hpp"
#include "../utils/uuid.hpp"

namespace message {
    WhoIsThis createWhoIsThis(const WhoIsThis_Sender& sender);
    ProposerToAcceptor createP1A(int id, int ballotNum, const std::string& acceptorGroupId, const std::string& ipAddress);
    AcceptorToProxyLeader createP1B(int messageId, const std::string& acceptorGroupId, const Ballot& highestBallot,
                                    const Log::pValueLog& log);
    ProposerToAcceptor createP2A(int id, int ballotNum, int slot, const Batch& payload,
                                 const std::string& acceptorGroupId, const std::string& ipAddress);
    AcceptorToProxyLeader createP2B(int messageId, const std::string& acceptorGroupId, const Ballot& highestBallot, int slot);
    ProxyLeaderToProposer createProxyP1B(int messageId, const std::string& acceptorGroupId, const Ballot& highestBallot,
                                         const Log::stringLog& committedLog, const Log::pValueLog& uncommittedLog);
    ProxyLeaderToProposer createProxyP2B(int messageId, const std::string& acceptorGroupId, const Ballot& highestBallot,
                                         int slot);
    ProxyLeaderToProposer createProxyLeaderHeartbeat();
    Heartbeat createGenericHeartbeat();
    ProposerToProposer createIamLeader();
    ClientToBatcher createClientRequest(const std::string& ipAddress, const std::string& payload);
    Batch createBatchMessage(const std::string& ipAddress, const std::vector<std::string>& requests);
    UnbatcherToClient createUnbatcherToClientAck(const std::string& request);
}

#endif //AUTOSCALING_PAXOS_MESSAGE_HPP
