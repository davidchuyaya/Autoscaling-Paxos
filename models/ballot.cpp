//
// Created by David Chu on 10/8/20.
//

#include "ballot.hpp"

int ballot::operator<(ballot otherBallot) const {
    if (ballotNum < otherBallot.ballotNum)
        return true;
    else if (ballotNum == otherBallot.ballotNum)
        return id < otherBallot.id;
    return false;
}
