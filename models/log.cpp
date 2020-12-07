//
// Created by David Chu on 10/6/20.
//

#include "log.hpp"

std::tuple<std::queue<int>, int>
Log::findHolesInLog(const stringLog& committedLog, const pValueLog& uncommittedLog) {
    std::queue<int> holes = {};
    int count = 0;
    int slot = 1; //log is 1-indexed, because 0 = null in protobuf & will be ignored
    while (count < (committedLog.size() + uncommittedLog.size())) {
        if (committedLog.find(slot) == committedLog.end() && uncommittedLog.find(slot) == uncommittedLog.end())
            holes.emplace(slot);
        else
            count += 1;
        slot += 1;
    }
    return {holes, slot};
}

Log::stringLog Log::mergeCommittedLogs(const std::vector<stringLog>& committedLogs) {
    stringLog committedLog = {};
    for (const stringLog& log : committedLogs)
        committedLog.insert(log.begin(), log.end());
    return committedLog;
}

std::tuple<Log::pValueLog, std::unordered_map<int, std::string>>
Log::mergeUncommittedLogs(const std::unordered_map<std::string, pValueLog>& uncommittedLogs) {
    std::unordered_map<int, std::string> acceptorGroupForSlot = {};
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
            if (pValue.payload().client().empty())
                continue;

            const PValue& bestValue = bestUncommittedValueForSlot[slot];
            if (google::protobuf::util::MessageDifferencer::Equals(bestValue, pValue)) {
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
        out << slot << ") " << pValue.payload().ShortDebugString() << ", ";
    return out.str();
}

bool Log::isBallotGreaterThan(const Ballot& ballotLeft, const Ballot& ballotRight) {
    return ballotLeft.ballotnum() > ballotRight.ballotnum() ||
           (ballotLeft.ballotnum() == ballotRight.ballotnum() && ballotLeft.id() > ballotRight.id());
}