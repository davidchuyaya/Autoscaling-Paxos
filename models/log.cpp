//
// Created by David Chu on 10/6/20.
//

#include <google/protobuf/util/message_differencer.h>
#include "log.hpp"

std::tuple<std::vector<std::string>, std::unordered_map<int, PValue>, std::unordered_map<int, int>>
Log::committedAndUncommittedLog(const std::unordered_map<int, std::vector<std::vector<PValue>>>& acceptorGroupLogs) {
    std::unordered_map<int, int> acceptorGroupForSlot = {};
    std::unordered_map<int, PValue> bestUncommittedValueForSlotInAllGroups = {};
    std::vector<std::string> bestCommittedValueForSlotInAllGroups = {};

    for (const auto& [acceptorGroupId, logs] : acceptorGroupLogs) {
        std::vector<PValue> bestValueForSlot = {};
        std::vector<int> countForSlot = {};
        bestValueForSlot.reserve(logs[0].size());
        countForSlot.reserve(logs[0].size());

        for (const auto& log : logs) {
            for (int slot = 0; slot < log.size(); slot++) {
                const PValue& pValue = log[slot];
                if (pValue.payload().empty())
                    continue;

                if (slot >= bestValueForSlot.size()) {
                    //we're the first value for this slot, automatically we're the best
                    // TODO make all logs maps
                    bestValueForSlot.resize(slot + 1);
                    countForSlot.resize(slot + 1);
                    bestValueForSlot[slot] = pValue;
                    countForSlot[slot] = 1;
                } else {
                    const PValue& bestValue = bestValueForSlot[slot];
                    if (bestValue.payload() == pValue.payload()) {
                        countForSlot[slot] += 1;
                    } else if (isBallotGreaterThan(pValue.ballot(), bestValue.ballot())) {
                        bestValueForSlot[slot] = pValue;
                        countForSlot[slot] = 1;
                    }
                }
            }
        }

        //coalesce committed/best values across acceptor groups
        for (int slot = 0; slot < bestValueForSlot.size(); slot++) {
            const PValue& pValue = bestValueForSlot[slot];
            const int count = countForSlot[slot];

            if (count == logs.size()) { // this PValue is committed
                if (bestCommittedValueForSlotInAllGroups.size() <= slot)
                    bestCommittedValueForSlotInAllGroups.resize(slot + 1);
                bestCommittedValueForSlotInAllGroups[slot] = pValue.payload();
                bestUncommittedValueForSlotInAllGroups.erase(slot);
                acceptorGroupForSlot[slot] = acceptorGroupId;
            }
            else {
                if (pValue.payload().empty())
                    continue;
                if (bestCommittedValueForSlotInAllGroups.size() > slot) {
                    const std::string& bestPayload = bestCommittedValueForSlotInAllGroups[slot];
                    if (!bestPayload.empty())
                        continue;
                }
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

void Log::printLog(const std::vector<PValue>& log) {
    for (int slot = 0; slot < log.size(); ++slot)
        printf("%d) %s\n", slot, log[slot].payload().c_str());
}

bool Log::isBallotGreaterThan(const Ballot& ballotLeft, const Ballot& ballotRight) {
    return ballotLeft.ballotnum() > ballotRight.ballotnum() ||
           (ballotLeft.ballotnum() == ballotRight.ballotnum() && ballotLeft.id() > ballotRight.id());
}