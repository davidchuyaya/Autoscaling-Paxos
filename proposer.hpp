//
// Created by David Chu on 10/4/20.
//

#ifndef C__PAXOS_PROPOSER_HPP
#define C__PAXOS_PROPOSER_HPP


#include "utils/networkNode.hpp"

class proposer: networkNode {
public:
    proposer();
    void startListening();
};


#endif //C__PAXOS_PROPOSER_HPP
