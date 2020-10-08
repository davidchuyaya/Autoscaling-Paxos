//
// Created by David Chu on 10/6/20.
//

#ifndef AUTOSCALING_PAXOS_MESSAGE_HPP
#define AUTOSCALING_PAXOS_MESSAGE_HPP

#include <string>
#include "message.pb.h"
#include "../log.hpp"

namespace message {
    ProposerToAcceptor createP1A(int id, int ballotNum);
    AcceptorToProposer createP1B(int id, int ballotNum);
    ProposerToAcceptor createP2A(int id, int ballotNum, const std::string& payload);
    AcceptorToProposer createP2B(int id, int ballotNum, Log log);
}


#endif //AUTOSCALING_PAXOS_MESSAGE_HPP
