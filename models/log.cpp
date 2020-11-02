//
// Created by David Chu on 10/6/20.
//

#include <google/protobuf/util/message_differencer.h>
#include <sstream>
#include "log.hpp"

void Log::mergeCommittedLogs(stringLog* committedLog, const std::vector<stringLog>& committedLogs) {
    for (const stringLog& log : committedLogs)
        committedLog->insert(log.begin(), log.end());
}

std::tuple<Log::pValueLog, std::unordered_map<int, int>>
Log::mergeUncommittedLogs(const std::unordered_map<int, pValueLog>& uncommittedLogs) {
    std::unordered_map<int, int> acceptorGroupForSlot = {};
    pValueLog bestUncommittedValueForSlot = {};

    for (const auto& [acceptorGroupId, logs] : uncommittedLogs) {
        for (const auto& [slot, pValue] : logs) {
            const PValue& prevBestPValue = bestUncommittedValueForSlot[slot];
            if (isBallotGreaterThan(pValue.ballot(), prevBestPValue.ballot())) { // we have the best uncommitted PValue
                bestUncommittedValueForSlot[slot] = pValue;
                acceptorGroupForSlot[slot] = acceptorGroupId;
            }
        }
    }

    return {bestUncommittedValueForSlot, acceptorGroupForSlot};
}

std::tuple<Log::stringLog, Log::pValueLog> Log::mergeLogsOfAcceptorGroup(const acceptorGroupLog& logs) {
    stringLog bestCommittedValueForSlot = {};
    pValueLog bestUncommittedValueForSlot = {};
    std::unordered_map<int, int> countForSlot = {}; //key = slot

    for (const auto& log : logs) {
        for (const auto& [slot, pValue] : log) {
            if (pValue.payload().empty())
                continue;

            const PValue& bestValue = bestUncommittedValueForSlot[slot];
            if (bestValue.payload() == pValue.payload()) {
                countForSlot[slot] += 1;
                if (countForSlot[slot] == logs.size()) {
                    //the value is committed
                    bestCommittedValueForSlot[slot] = pValue.payload();
                    bestUncommittedValueForSlot.erase(slot);
                }
            } else if (isBallotGreaterThan(pValue.ballot(), bestValue.ballot())) {
                bestUncommittedValueForSlot[slot] = pValue;
                countForSlot[slot] = 1;
            }
        }
    }

    return {bestCommittedValueForSlot, bestUncommittedValueForSlot};
}

std::string Log::printLog(const pValueLog& log) {
    std::stringstream out;
    for (const auto&[slot, pValue] : log)
        out << slot << ") " << pValue.payload().c_str() << ", ";
    return out.str();
}

bool Log::isBallotGreaterThan(const Ballot& ballotLeft, const Ballot& ballotRight) {
    return ballotLeft.ballotnum() > ballotRight.ballotnum() ||
           (ballotLeft.ballotnum() == ballotRight.ballotnum() && ballotLeft.id() > ballotRight.id());
}