//
// Created by David Chu on 10/6/20.
//

#include "message.hpp"

ProposerToAcceptor message::createP1A(int id, int ballotNum) {
    ProposerToAcceptor p1a;
    p1a.set_type(ProposerToAcceptor_Type_p1a);
    p1a.set_id(id);
    p1a.set_ballot(ballotNum);
    return p1a;
}

AcceptorToProposer message::createP1B(ballot highestBallot) {
    AcceptorToProposer p1b;
    p1b.set_type(AcceptorToProposer_Type_p1b);
    p1b.set_id(highestBallot.id);
    p1b.set_ballot(highestBallot.ballotNum);
    return p1b;
}

ProposerToAcceptor message::createP2A(int id, int ballotNum, const std::string& payload) {
    return ProposerToAcceptor();
}

AcceptorToProposer message::createP2B(int id, int ballotNum, Log log) {
    return AcceptorToProposer();
}