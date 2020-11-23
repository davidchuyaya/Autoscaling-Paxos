//
// Created by David Chu on 10/4/20.
//
#include "proposer.hpp"

proposer::proposer(const int id, const int numAcceptorGroups) : id(id), proxyLeaders(config::F+1), proposers(config::F+1) {
    std::thread server([&] { startServer(); });
    server.detach();
    proposers.addSelfAsConnection();
    annaClient = new anna(config::KEY_PROPOSERS,{config::KEY_PROPOSERS, config::KEY_ACCEPTOR_GROUPS},
                    [&](const std::string& key, const two_p_set& twoPSet) {
                        listenToAnna(key, twoPSet);
    });

    //wait for acceptor group IDs before starting phase 1 or phase 2. All batches will be dropped until then.
    std::unique_lock lock(acceptorMutex);
    acceptorCV.wait(lock, [&]{ return acceptorGroupIds.size() >= numAcceptorGroups; });
    lock.unlock();

    std::thread checkLeader([&] { leaderLoop(); });
    checkLeader.detach();
    pthread_exit(nullptr);
}

[[noreturn]]
void proposer::leaderLoop() {
    const ProposerToProposer& iAmLeader = message::createIamLeader();
    time_t now;
    while (true) {
        time(&now);

        //send heartbeats
        if (isLeader) {
            LOG("%d = leader, sending at time: %s\n", id, std::asctime(std::localtime(&now)));
            proposers.broadcast(iAmLeader);
        }
        //receive heartbeats, timeout existing leaders
        else {
            std::shared_lock heartbeatLock(heartbeatMutex);
            if (difftime(now, lastLeaderHeartbeat) > config::HEARTBEAT_TIMEOUT_SEC) {
                heartbeatLock.unlock();
                //Note: scouts are resent with the frequency of HEARTBEAT_SLEEP_SEC unless a new leader is detected or we are it
                sendScouts();
            }
        }

        std::this_thread::sleep_for(std::chrono::seconds(config::HEARTBEAT_SLEEP_SEC));
    }
}

void proposer::listenToAnna(const std::string& key, const two_p_set& twoPSet) {
    if (key == config::KEY_PROPOSERS) {
        // connect to new proposer
        proposers.connectAndMaybeListen(twoPSet, config::PROPOSER_PORT_START, WhoIsThis_Sender_proposer,
                                        [&](const int socket, const std::string& payload){
                                            listenToProposer();
        });
    } else if (key == config::KEY_ACCEPTOR_GROUPS) {
        // merge new acceptor group IDs
        const two_p_set& updates = acceptorGroupIdSet.updatesFrom(twoPSet);

        std::unique_lock lock(acceptorMutex);
        for (const std::string& acceptorGroupId : updates.getObserved()) {
            acceptorGroupIds.emplace_back(acceptorGroupId);
            //TODO if leader, attempt to win matchmakers with new configuration
        }
        for (const std::string& acceptorGroupId : updates.getRemoved()) {
            acceptorGroupIds.erase(std::remove(acceptorGroupIds.begin(), acceptorGroupIds.end(), acceptorGroupId), acceptorGroupIds.end());
        }
        acceptorGroupIdSet.merge(updates);
    }
}

[[noreturn]]
void proposer::startServer() {
    network::startServerAtPort(config::PROPOSER_PORT_START,
           [&](const int socket, const WhoIsThis_Sender& whoIsThis) {
            switch (whoIsThis) {
                case WhoIsThis_Sender_batcher:
                    LOG("Server %d connected to batcher\n", id);
                    break;
                case WhoIsThis_Sender_proxyLeader:
                    LOG("Server %d connected to proxy leader\n", id);
                    proxyLeaders.addConnection(socket);
                    break;
                case WhoIsThis_Sender_proposer: {
                    LOG("Server %d connected to proposer\n", id);
                    proposers.addConnection(socket);
                    break;
                }
                default: {}
            }
        }, [&](const int socket, const WhoIsThis_Sender& whoIsThis, const std::string& payloadString) {
            switch (whoIsThis) {
                case WhoIsThis_Sender_batcher:
                    listenToBatcher(payloadString);
                    break;
                case WhoIsThis_Sender_proxyLeader: {
                    ProxyLeaderToProposer payload;
                    payload.ParseFromString(payloadString);
                    listenToProxyLeader(socket, payload);
                    break;
                }
                case WhoIsThis_Sender_proposer:
                    listenToProposer();
                    break;
                default: {}
            }
    });
}

void proposer::listenToBatcher(const std::string& payload) {
    LOG("Proposer %d received a batch request\n", id);
    if (!isLeader)
        return;

    std::shared_lock acceptorsLock(acceptorMutex, std::defer_lock);
    std::shared_lock ballotLock(ballotMutex, std::defer_lock);
    std::scoped_lock lock(logMutex, acceptorsLock, ballotLock);
    int slot;
    if (logHoles.empty()) {
        slot = nextSlot;
        nextSlot += 1;
    }
    else {
        slot = logHoles.front();
        logHoles.pop();
    }
    proxyLeaders.send(message::createP2A(id, ballotNum, slot, payload, fetchNextAcceptorGroupId(), config::IP_ADDRESS));
}

void proposer::listenToProxyLeader(const int socket, const ProxyLeaderToProposer& payload) {
    switch (payload.type()) {
        case ProxyLeaderToProposer_Type_p1b:
            handleP1B(payload);
            break;
        case ProxyLeaderToProposer_Type_p2b:
            handleP2B(payload);
            break;
        case ProxyLeaderToProposer_Type_heartbeat: {
            proxyLeaders.addHeartbeat(socket);
            break;
        }
        default: {}
    }
}

void proposer::listenToProposer() {
    std::unique_lock lock(heartbeatMutex);
    LOG("%d received leader heartbeat for time: %s\n", id, std::asctime(std::localtime(&lastLeaderHeartbeat)));
    time(&lastLeaderHeartbeat); // store the time we received the heartbeat
    lock.unlock();

    noLongerLeader();
}

void proposer::sendScouts() {
    // random timeout so a leader is easily elected
    std::this_thread::sleep_for(std::chrono::seconds(id * config::ID_SCOUT_DELAY_MULTIPLIER));

    int currentBallotNum;
    std::unique_lock ballotLock(ballotMutex);
    ballotNum += 1;
    currentBallotNum = ballotNum;
    ballotLock.unlock();
    LOG("P1A blasting out: id = %d, ballotNum = %d\n", id, currentBallotNum);

    std::shared_lock acceptorsLock(acceptorMutex, std::defer_lock);
    std::shared_lock logLock(logMutex, std::defer_lock);
    std::scoped_lock lock(remainingAcceptorGroupsForScoutsMutex, logLock, acceptorsLock);
    for (const std::string& acceptorGroupId : acceptorGroupIds) {
        remainingAcceptorGroupsForScouts.emplace(acceptorGroupId);
        proxyLeaders.send(message::createP1A(id, currentBallotNum, acceptorGroupId, config::IP_ADDRESS));
    }
}

void proposer::handleP1B(const ProxyLeaderToProposer& message) {
    LOG("Proposer %d received p1b from acceptor group: %s, committed log length: %d, uncommitted log length: %d\n", id,
           message.acceptorgroupid().c_str(), message.committedlog_size(), message.uncommittedlog_size());

    if (message.ballot().id() != id) { // we lost the election
        // store the largest ballot we last saw so we can immediately catch up
        std::unique_lock lock(ballotMutex);
        ballotNum = message.ballot().ballotnum();
        lock.unlock();
        noLongerLeader();
        return;
    }

    {std::scoped_lock lock(acceptorGroupLogsMutex, remainingAcceptorGroupsForScoutsMutex);
    acceptorGroupCommittedLogs.emplace_back(message.committedlog().begin(), message.committedlog().end());
    acceptorGroupUncommittedLogs[message.acceptorgroupid()] =
            {message.uncommittedlog().begin(), message.uncommittedlog().end()};
    remainingAcceptorGroupsForScouts.erase(message.acceptorgroupid());

    if (!remainingAcceptorGroupsForScouts.empty()) //we're still waiting for other acceptor groups to respond
        return;}

    //leader election complete
    isLeader = true;
    LOG("Proposer %d is leader\n", id);
    proposers.broadcast(message::createIamLeader());

    mergeLogs();
}

void proposer::mergeLogs() {
    std::shared_lock ballotLock(ballotMutex, std::defer_lock);
    std::shared_lock acceptorLock(acceptorMutex, std::defer_lock);
    std::scoped_lock lock(acceptorGroupLogsMutex, logMutex, ballotLock, acceptorLock);
    const Log::stringLog& committedLog = Log::mergeCommittedLogs(acceptorGroupCommittedLogs);
    const auto& [uncommittedLog, acceptorGroupForSlot] = Log::mergeUncommittedLogs(acceptorGroupUncommittedLogs);
    acceptorGroupCommittedLogs.clear();
    acceptorGroupUncommittedLogs.clear();

    //calculate which slots we're allowed to assign in the future
    const auto& [tempLogHoles, tempNextSlot] = Log::findHolesInLog(committedLog, uncommittedLog);
    logHoles = tempLogHoles;
    nextSlot = tempNextSlot;

    //resend uncommitted messages
    for (const auto& [slot, pValue] : uncommittedLog)
        proxyLeaders.send(message::createP2A(id, ballotNum, slot, pValue.payload(), acceptorGroupForSlot.at(slot),
											 config::IP_ADDRESS));
}

void proposer::handleP2B(const ProxyLeaderToProposer& message) {
    LOG("Proposer %d received p2b, highest ballot: [%d, %d]\n", id, message.ballot().id(), message.ballot().ballotnum());
    if (message.ballot().id() != id) { //yikes, we got preempted
        noLongerLeader();
    }
}

void proposer::noLongerLeader() {
    LOG("Proposer %d is no longer the leader\n", id);
    isLeader = false;

    std::scoped_lock lock(acceptorGroupLogsMutex, remainingAcceptorGroupsForScoutsMutex);
    acceptorGroupCommittedLogs.clear();
    acceptorGroupUncommittedLogs.clear();
    remainingAcceptorGroupsForScouts.clear();
}

const std::string& proposer::fetchNextAcceptorGroupId() {
    nextAcceptorGroup = (nextAcceptorGroup + 1) % acceptorGroupIds.size();
    return acceptorGroupIds[nextAcceptorGroup];
}


int main(const int argc, const char** argv) {
    if (argc != 3) {
        printf("Usage: ./proposer <PROPOSER ID> <NUM ACCEPTOR GROUPS>.\n");
        exit(0);
    }
    const int id = std::stoi( argv[1] );
	const int numAcceptorGroups = std::stoi(argv[2]);
    proposer p(id, numAcceptorGroups);
}

