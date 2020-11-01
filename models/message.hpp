//
// Created by David Chu on 10/6/20.
//

#ifndef AUTOSCALING_PAXOS_MESSAGE_HPP
#define AUTOSCALING_PAXOS_MESSAGE_HPP

#include <string>
#include "message.pb.h"
#include "log.hpp"

namespace message {
    ProposerToAcceptor createP1A(int id, int ballotNum);
    AcceptorToProposer createP1B(int acceptorGroupId, const Ballot& highestBallot, const Log::pValueLog& log);
    ProposerToAcceptor createP2A(int id, int ballotNum, int slot, const std::string& payload);
    AcceptorToProposer createP2B(const Ballot& highestBallot, int acceptorGroupId, int slot);
    ProposerReceiver createIamLeader();
    ProposerReceiver createBatchMessage(const std::vector<std::string>& requests);
}

#endif //AUTOSCALING_PAXOS_MESSAGE_HPP