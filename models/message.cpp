//
// Created by David Chu on 10/6/20.
//

#include "message.hpp"

ProposerToAcceptor message::createP1A(const int id, const int ballotNum) {
    ProposerToAcceptor p1a;
    p1a.set_type(ProposerToAcceptor_Type_p1a);
    Ballot* ballot = p1a.mutable_ballot();
    ballot->set_id(id);
    ballot->set_ballotnum(ballotNum);
    return p1a;
}

AcceptorToProposer message::createP1B(const Ballot& highestBallot, const std::vector<PValue>& log) {
    AcceptorToProposer p1b;
    p1b.set_type(AcceptorToProposer_Type_p1b);
    *p1b.mutable_ballot() = highestBallot;
    *p1b.mutable_log() = {log.begin(), log.end()};
    return p1b;
}

ProposerToAcceptor message::createP2A(const int id, const int ballotNum, const int slot, const std::string& payload) {
    ProposerToAcceptor p2a;
    p2a.set_type(ProposerToAcceptor_Type_p2a);
    Ballot* ballot = p2a.mutable_ballot();
    ballot->set_id(id);
    ballot->set_ballotnum(ballotNum);
    p2a.set_slot(slot);
    p2a.set_payload(payload);
    return p2a;
}

AcceptorToProposer message::createP2B(const Ballot& highestBallot, const int slot) {
    AcceptorToProposer p2b;
    p2b.set_type(AcceptorToProposer_Type_p2b);
    *p2b.mutable_ballot() = highestBallot;
    p2b.set_slot(slot);
    return p2b;
}