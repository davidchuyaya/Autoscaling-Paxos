//
// Created by David Chu on 10/6/20.
//

#include <google/protobuf/util/message_differencer.h>
#include "log.hpp"

std::tuple<std::vector<std::string>, std::unordered_map<int, std::string>>
Log::committedAndUncommittedLog(const std::vector<std::vector<PValue>>& acceptorLogs) {
    std::vector<PValue> bestValueForSlot = {};
    std::vector<int> countForSlot = {};
    bestValueForSlot.reserve(acceptorLogs[0].size());
    countForSlot.reserve(acceptorLogs[0].size());

    for (const auto& log : acceptorLogs) {
        for (int slot = 0; slot < log.size(); slot++) {
            const PValue &pValue = log[slot];
            if (pValue.payload().empty())
                continue;

            if (slot >= bestValueForSlot.size()) {
                //we're the first value for this slot, automatically we're the best
                bestValueForSlot.resize(slot + 1);
                countForSlot.resize(slot + 1);
                bestValueForSlot[slot] = pValue;
                countForSlot[slot] = 1;
            }
            else {
                const PValue& bestValue = bestValueForSlot[slot];
                if (bestValue.payload() == pValue.payload()) {
                    countForSlot[slot] += 1;
                }
                else if (isBallotGreaterThan(pValue.ballot(), bestValue.ballot())) {
                    bestValueForSlot[slot] = pValue;
                    countForSlot[slot] = 1;
                }
            }
        }
    }

    std::vector<std::string> committedLog = {};
    std::unordered_map<int, std::string> uncommittedLog = {};
    committedLog.reserve(bestValueForSlot.size());
    uncommittedLog.reserve(bestValueForSlot.size());

    for (int slot = 0; slot < bestValueForSlot.size(); slot++) {
        const std::string& payload = bestValueForSlot[slot].payload();
        const int count = countForSlot[slot];

        if (count == acceptorLogs.size()) {
            if (committedLog.size() <= slot)
                committedLog.resize(slot + 1);
            committedLog[slot] = payload;
        }
        else
            uncommittedLog[slot] = payload;
    }
    return {committedLog, uncommittedLog};
}

void Log::printLog(const std::vector<PValue>& log) {
    for (int slot = 0; slot < log.size(); ++slot)
        printf("%d) %s\n", slot, log[slot].payload().c_str());
}

bool Log::isBallotGreaterThan(const Ballot& ballotLeft, const Ballot& ballotRight) {
    return ballotLeft.ballotnum() > ballotRight.ballotnum() ||
           (ballotLeft.ballotnum() == ballotRight.ballotnum() && ballotLeft.id() > ballotRight.id());
}