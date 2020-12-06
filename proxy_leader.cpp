//
// Created by David Chu on 10/29/20.
//

#include "proxy_leader.hpp"

proxy_leader::proxy_leader() :
unbatchers(config::F+1, config::UNBATCHER_PORT, WhoIsThis_Sender_proxyLeader,[&](const int socket, const Heartbeat& payload) {
	unbatchers.addHeartbeat(socket);
}),
proposers(config::F+1, config::PROPOSER_PORT,WhoIsThis_Sender_proxyLeader,
  [&](const int socket, const ProposerToAcceptor& payload) {
	listenToProposer(payload);
}) {
	annaClient = anna::readWritable({{config::KEY_PROXY_LEADERS, config::IP_ADDRESS}},
					   [&](const std::string& key, const two_p_set& twoPSet) {
		listenToAnna(key, twoPSet);
	});
	annaClient->subscribeTo(config::KEY_PROPOSERS);
	annaClient->subscribeTo(config::KEY_UNBATCHERS);
    heartbeater::mainThreadHeartbeat(message::createProxyLeaderHeartbeat(), proposers);
}

void proxy_leader::listenToAnna(const std::string& key, const two_p_set& twoPSet) {
    if (key == config::KEY_PROPOSERS) {
        proposers.connectAndMaybeListen(twoPSet);
        if (proposers.twoPsetThresholdMet())
	        annaClient->unsubscribeFrom(config::KEY_PROPOSERS);
    }
    else if (key == config::KEY_UNBATCHERS) {
        unbatchers.connectAndMaybeListen(twoPSet);
    }
    else {
        //must be individual acceptor groups
        processAcceptors(key, twoPSet);
    }
}

void proxy_leader::processNewAcceptorGroup(const std::string& acceptorGroupId) {
	std::unique_lock lock(acceptorMutex);
	LOG("New acceptor group from proposer: {}", acceptorGroupId);
	acceptorGroupSockets[acceptorGroupId] = new threshold_component<ProposerToAcceptor, AcceptorToProxyLeader>
			(2 * config::F + 1, config::ACCEPTOR_PORT,WhoIsThis_Sender_proxyLeader,
	[&](const int socket, const AcceptorToProxyLeader& payload) {
				listenToAcceptor(payload);
			});
	lock.unlock();

	annaClient->subscribeTo(acceptorGroupId); //find the IP addresses of acceptors in this group
}

void proxy_leader::processAcceptors(const std::string& acceptorGroupId, const two_p_set& twoPSet) {
	std::shared_lock readLock(acceptorMutex);
	threshold_component<ProposerToAcceptor, AcceptorToProxyLeader>* acceptors = acceptorGroupSockets.at(acceptorGroupId);
	acceptors->connectAndMaybeListen(twoPSet);
	// stop requesting for acceptors once we see 2f+1
	if (acceptors->twoPsetThresholdMet())
		annaClient->unsubscribeFrom(acceptorGroupId);
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
	    processNewAcceptorGroup(payload.acceptorgroupid());
	    readLock.lock();
    }
    acceptorGroupSockets.at(payload.acceptorgroupid())->broadcast(payload);
	TIME();
}

void proxy_leader::listenToAcceptor(const AcceptorToProxyLeader& payload) {
    LOG("Received from acceptors: {}\n", payload.ShortDebugString());

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
	    BENCHMARK_LOG("P1B preempted for proposer {}\n", payload.ballot().id());
        const ProxyLeaderToProposer& messageToProposer = message::createProxyP1B(payload.messageid(),
                                                                                 payload.acceptorgroupid(),
                                                                                 payload.ballot(), {}, {});
	    proposers.sendToIp(sentValue.ipaddress(), messageToProposer);
        sentMessages.erase(payload.messageid());
        unmergedLogs.erase(payload.messageid());
    }
    else {
        //add another log
        unmergedLogs[payload.messageid()].emplace_back(payload.log().begin(), payload.log().end());

        if (unmergedLogs[payload.messageid()].size() >= config::F + 1) {
            //we have f+1 good logs, merge them & tell the proposer
	        BENCHMARK_LOG("P1B approved for proposer {}\n", payload.ballot().id());
            const auto&[committedLog, uncommittedLog] = Log::mergeLogsOfAcceptorGroup(unmergedLogs[payload.messageid()]);
            const ProxyLeaderToProposer& messageToProposer = message::createProxyP1B(payload.messageid(),
																					 payload.acceptorgroupid(),
                                                                                     payload.ballot(),
                                                                                     committedLog, uncommittedLog);
	        proposers.sendToIp(sentValue.ipaddress(), messageToProposer);
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
	    proposers.sendToIp(sentValue.ipaddress(), messageToProposer);
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

    INIT_LOGGER();
	network::ignoreClosedSocket();
	proxy_leader pr {};
}
