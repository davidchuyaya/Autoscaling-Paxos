//
// Created by David Chu on 12/31/20.
//

#ifndef AUTOSCALING_PAXOS_COMPONENTS_HPP
#define AUTOSCALING_PAXOS_COMPONENTS_HPP


enum ComponentType {
	Client, Batcher, Proposer, ProxyLeader, Acceptor, Unbatcher, Matchmaker
};


#endif //AUTOSCALING_PAXOS_COMPONENTS_HPP
