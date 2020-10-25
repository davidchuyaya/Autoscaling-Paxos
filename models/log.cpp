//
// Created by David Chu on 10/6/20.
//

#include <google/protobuf/util/message_differencer.h>
#include <sstream>
#include "log.hpp"

std::tuple<Log::stringLog, Log::pValueLog, std::unordered_map<int, int>>
Log::committedAndUncommittedLog(const allAcceptorGroupLogs& acceptorGroupLogs) {
    std::unordered_map<int, int> acceptorGroupForSlot = {};
    pValueLog bestUncommittedValueForSlotInAllGroups = {};
    stringLog bestCommittedValueForSlotInAllGroups = {};

    for (const auto& [acceptorGroupId, logs] : acceptorGroupLogs) {
        pValueLog bestValueForSlot = {};
        std::unordered_map<int, int> countForSlot = {}; //key = slot
        bestValueForSlot.reserve(logs[0].size());
        countForSlot.reserve(logs[0].size());

        for (const auto& log : logs) {
            for (const auto& [slot, pValue] : log) {
                if (pValue.payload().empty())
                    continue;

                const PValue& bestValue = bestValueForSlot[slot];
                if (bestValue.payload() == pValue.payload()) {
                    countForSlot[slot] += 1;
                } else if (isBallotGreaterThan(pValue.ballot(), bestValue.ballot())) {
                    bestValueForSlot[slot] = pValue;
                    countForSlot[slot] = 1;
                }
            }
        }

        //coalesce committed/best values across acceptor groups
        for (const auto& [slot, pValue] : bestValueForSlot) {
            const int count = countForSlot[slot];

            if (count == logs.size()) { // this PValue is present in all acceptors of the group (it is committed)
                bestCommittedValueForSlotInAllGroups[slot] = pValue.payload();
                bestUncommittedValueForSlotInAllGroups.erase(slot);
                acceptorGroupForSlot[slot] = acceptorGroupId;
            }
            else {
                if (pValue.payload().empty())
                    continue;
                if (!bestCommittedValueForSlotInAllGroups[slot].empty()) //there's already a committed value at this slot
                    continue;
                const PValue& prevBestPValue = bestUncommittedValueForSlotInAllGroups[slot];
                if (isBallotGreaterThan(pValue.ballot(), prevBestPValue.ballot())) { // we have the best uncommitted PValue
                    bestUncommittedValueForSlotInAllGroups[slot] = pValue;
                    acceptorGroupForSlot[slot] = acceptorGroupId;
                }
            }
        }
    }
    return {bestCommittedValueForSlotInAllGroups, bestUncommittedValueForSlotInAllGroups, acceptorGroupForSlot};
}

void Log::printLog(const pValueLog& log) {
    std::stringstream out;
    for (const auto&[slot, pValue] : log)
        out << slot << ") " << pValue.payload().c_str() << '\n';
    std::cout << out.str() << std::endl;
}

bool Log::isBallotGreaterThan(const Ballot& ballotLeft, const Ballot& ballotRight) {
    return ballotLeft.ballotnum() > ballotRight.ballotnum() ||
           (ballotLeft.ballotnum() == ballotRight.ballotnum() && ballotLeft.id() > ballotRight.id());
}