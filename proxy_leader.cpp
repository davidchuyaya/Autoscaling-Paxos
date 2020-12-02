//
// Created by David Chu on 10/29/20.
//

#include "proxy_leader.hpp"

proxy_leader::proxy_leader() : unbatchers(config::F+1), proposers(config::F+1) {
	annaClient = new anna{config::KEY_PROXY_LEADERS,
					   {config::KEY_PROPOSERS, config::KEY_UNBATCHERS, config::KEY_ACCEPTOR_GROUPS},
					   [&](const std::string& key, const two_p_set& twoPSet) {
		listenToAnna(key, twoPSet);
	}};
    heartbeater::mainThreadHeartbeat(message::createProxyLeaderHeartbeat(), proposers);
}

void proxy_leader::listenToAnna(const std::string& key, const two_p_set& twoPSet) {
    if (key == config::KEY_PROPOSERS) {
        proposers.connectAndMaybeListen<ProposerToAcceptor>(twoPSet, config::PROPOSER_PORT,WhoIsThis_Sender_proxyLeader,
                                        [&](const int socket, const ProposerToAcceptor& payload) {
            listenToProposer(payload);
        });
    }
    else if (key == config::KEY_UNBATCHERS) {
        unbatchers.connectAndListen<Heartbeat>(twoPSet, config::UNBATCHER_PORT, WhoIsThis_Sender_proxyLeader,
                                    [&](const int socket, const Heartbeat& payload) {
            unbatchers.addHeartbeat(socket);
        });
    }
    else if (key == config::KEY_ACCEPTOR_GROUPS) {
        processAcceptorGroup(twoPSet);
    }
    else {
        //must be individual acceptor groups
        processAcceptors(key, twoPSet);
    }
}

void proxy_leader::processAcceptorGroup(const two_p_set& twoPSet) {
    const two_p_set& updates = acceptorGroupIdSet.updatesFrom(twoPSet);
    for (const std::string& newAcceptorGroupId : updates.getObserved()) {
        //find the IP addresses of acceptors in this group
        annaClient->subscribeTo(newAcceptorGroupId);
    }
    for (const std::string& removedAcceptorGroupId : updates.getRemoved()) {
        annaClient->unsubscribeFrom(removedAcceptorGroupId);
        //create 2p-set with all members removed, then merge it in to close all sockets. Then let it be GC'd
	    std::unique_lock lock(acceptorMutex);
        if (!knowOfAcceptorGroup(removedAcceptorGroupId))
	        continue;
        threshold_component* acceptors = acceptorGroupSockets.at(removedAcceptorGroupId);
        two_p_set allMembersRemoved = {{}, acceptors->getMembers().getObserved()};
        acceptors->connectAndMaybeListen<AcceptorToProxyLeader>(allMembersRemoved, config::ACCEPTOR_PORT,
										WhoIsThis_Sender_proxyLeader, {});
        acceptorGroupSockets.erase(removedAcceptorGroupId);
	    free(acceptors);
    }
    acceptorGroupIdSet.merge(updates);
}

void proxy_leader::processAcceptors(const std::string& acceptorGroupId, const two_p_set& twoPSet) {
	// if we've never heard from this acceptor group before, create the threshold_component
	std::shared_lock readLock(acceptorMutex);
	if (!knowOfAcceptorGroup(acceptorGroupId)) {
		readLock.unlock(); //convoluted R/W lock scheme. Only 1 out of 2f+1 will need to write, so this should make it faster.
		std::unique_lock writeLock(acceptorMutex);
		acceptorGroupSockets[acceptorGroupId] = new threshold_component{2 * config::F + 1};
		writeLock.unlock();
		readLock.lock();
	}

	threshold_component* acceptors = acceptorGroupSockets.at(acceptorGroupId);
	acceptors->connectAndMaybeListen<AcceptorToProxyLeader>(twoPSet, config::ACCEPTOR_PORT,WhoIsThis_Sender_proxyLeader,
								 [&](const int socket, const AcceptorToProxyLeader& payload) {
		listenToAcceptor(payload);
	});
}

void proxy_leader::listenToProposer(const ProposerToAcceptor& payload) {
	LOG("Received from proposer: {}\n", payload.ShortDebugString());
	TIME();

    // keep track
    std::unique_lock messagesLock(sentMessagesMutex);
    sentMessages[payload.messageid()] = payload;
    messagesLock.unlock();

	// Broadcast to Acceptors; wait if necessary
    std::shared_lock readLock(acceptorMutex);
    if (!knowOfAcceptorGroup(payload.acceptorgroupid())) {
	    readLock.unlock(); //convoluted R/W lock scheme. Only happens once per acceptor group, should be fast.
	    std::unique_lock writeLock(acceptorMutex);
	    acceptorGroupSockets[payload.acceptorgroupid()] = new threshold_component{2 * config::F + 1};
	    writeLock.unlock();
	    annaClient->subscribeTo(payload.acceptorgroupid()); //find the IP addresses of acceptors in this group
	    readLock.lock();
    }
    acceptorGroupSockets.at(payload.acceptorgroupid())->broadcast(payload);
	TIME();
}

void proxy_leader::listenToAcceptor(const AcceptorToProxyLeader& payload) {
    LOG("Received from acceptors: {}\n", payload.ShortDebugString());
//	google::protobuf::util::ParseDelimitedFromCodedStream();

    switch (payload.type()) {
        case AcceptorToProxyLeader_Type_p1b:
            handleP1B(payload);
            break;
        case AcceptorToProxyLeader_Type_p2b:
            handleP2B(payload);
            break;
        default: {}
    }
}

void proxy_leader::handleP1B(const AcceptorToProxyLeader& payload) {
	std::scoped_lock lock(sentMessagesMutex, unmergedLogsMutex);
    const ProposerToAcceptor& sentValue = sentMessages[payload.messageid()];
    if (sentValue.ballot().ballotnum() == 0) {
        //p1b is arriving for a nonexistent sentValue
        return;
    }

    if (Log::isBallotGreaterThan(payload.ballot(), sentValue.ballot())) {
        //yikes, the proposer got preempted
	    LOG("P1B preempted for proposer {}\n", payload.ballot().id());
        const ProxyLeaderToProposer& messageToProposer = message::createProxyP1B(payload.messageid(),
                                                                                 payload.acceptorgroupid(),
                                                                                 payload.ballot(), {}, {});
        network::sendPayload(proposers.socketForIP(sentValue.ipaddress()), messageToProposer);
        sentMessages.erase(payload.messageid());
        unmergedLogs.erase(payload.messageid());
    }
    else {
        //add another log
        unmergedLogs[payload.messageid()].emplace_back(payload.log().begin(), payload.log().end());

        if (unmergedLogs[payload.messageid()].size() >= config::F + 1) {
            //we have f+1 good logs, merge them & tell the proposer
	        LOG("P1B approved for proposer {}\n", payload.ballot().id());
            const auto&[committedLog, uncommittedLog] = Log::mergeLogsOfAcceptorGroup(unmergedLogs[payload.messageid()]);
            const ProxyLeaderToProposer& messageToProposer = message::createProxyP1B(payload.messageid(),
																					 payload.acceptorgroupid(),
                                                                                     payload.ballot(),
                                                                                     committedLog, uncommittedLog);
            network::sendPayload(proposers.socketForIP(sentValue.ipaddress()), messageToProposer);
            sentMessages.erase(payload.messageid());
            unmergedLogs.erase(payload.messageid());
        }
    }
}

void proxy_leader::handleP2B(const AcceptorToProxyLeader& payload) {
	std::scoped_lock lock(sentMessagesMutex, approvedCommandersMutex);
    const ProposerToAcceptor& sentValue = sentMessages[payload.messageid()];
    if (sentValue.slot() == 0) {
        //p2b is arriving for a nonexistent sentValue
        return;
    }

    if (Log::isBallotGreaterThan(payload.ballot(), sentValue.ballot())) {
        //yikes, the proposer got preempted
	    LOG("P2B preempted for proposer: {}, slot: {}\n", payload.ballot().id(), payload.slot());
        const ProxyLeaderToProposer& messageToProposer = message::createProxyP2B(payload.messageid(), payload.acceptorgroupid(),
                                                                                payload.ballot(), payload.slot());
        network::sendPayload(proposers.socketForIP(sentValue.ipaddress()), messageToProposer);
        sentMessages.erase(payload.messageid());
        approvedCommanders.erase(payload.messageid());
    }
    else {
        //add another approved commander
        approvedCommanders[payload.messageid()] += 1;

        if (approvedCommanders[payload.messageid()] >= config::F + 1) {
            //we have f+1 approved commanders, tell the unbatcher. No need to tell proposer
	        LOG("P2B approved for proposer: {}, slot: {}\n", payload.ballot().id(), payload.slot());
	        Batch batch;
	        batch.ParseFromString(sentMessages[payload.messageid()].payload());
	        unbatchers.send(batch);
            sentMessages.erase(payload.messageid());
            approvedCommanders.erase(payload.messageid());
	        TIME();
        }
    }
}

bool proxy_leader::knowOfAcceptorGroup(const std::string& acceptorGroupId) {
	return acceptorGroupSockets.find(acceptorGroupId) != acceptorGroupSockets.end();
}

int main(int argc, char** argv) {
    if (argc != 1) {
        printf("Usage: ./proxy_leader\n");
        exit(0);
    }
    proxy_leader pr {};
}
