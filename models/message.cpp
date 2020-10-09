//
// Created by David Chu on 10/6/20.
//

#include "message.hpp"

ProposerToAcceptor message::createP1A(int id, int ballotNum) {
    ProposerToAcceptor p1a;
    p1a.set_type(ProposerToAcceptor_Type_p1a);
    Ballot* ballot = p1a.mutable_ballot();
    ballot->set_id(id);
    ballot->set_ballotnum(ballotNum);
    return p1a;
}

AcceptorToProposer message::createP1B(Ballot& highestBallot, const Log& log) {
    AcceptorToProposer p1b;
    p1b.set_type(AcceptorToProposer_Type_p1b);
    *p1b.mutable_ballot() = highestBallot;
    *p1b.mutable_log() = {log.pValues.begin(), log.pValues.end()};
    return p1b;
}

ProposerToAcceptor message::createP2A(int id, int ballotNum, const std::string& payload) {
    ProposerToAcceptor p2a;
    p2a.set_type(ProposerToAcceptor_Type_p2a);
    Ballot* ballot = p2a.mutable_ballot();
    ballot->set_id(id);
    ballot->set_ballotnum(ballotNum);
    p2a.set_payload(payload);
    return p2a;
}

AcceptorToProposer message::createP2B(Ballot& highestBallot) {
    AcceptorToProposer p2b;
    p2b.set_type(AcceptorToProposer_Type_p1b);
    *p2b.mutable_ballot() = highestBallot;
    return p2b;
}