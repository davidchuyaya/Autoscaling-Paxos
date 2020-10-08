//
// Created by David Chu on 10/8/20.
//

#ifndef AUTOSCALING_PAXOS_BALLOT_HPP
#define AUTOSCALING_PAXOS_BALLOT_HPP

struct ballot {
    int id;
    int ballotNum;

    int operator<(ballot otherBallot) const;
};


#endif //AUTOSCALING_PAXOS_BALLOT_HPP
