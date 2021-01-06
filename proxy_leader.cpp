//
// Created by David Chu on 10/29/20.
//

#include "proxy_leader.hpp"

proxy_leader::proxy_leader() {
	zmqNetwork = new network();

	annaClient = new anna(zmqNetwork, {{config::KEY_PROXY_LEADERS, config::IP_ADDRESS}},
					   [&](const std::string& key, const two_p_set& twoPSet, const time_t now) {
		listenToAnna(key, twoPSet, now);
	});
	annaClient->subscribeTo(config::KEY_PROPOSERS);
	annaClient->subscribeTo(config::KEY_ACCEPTOR_GROUPS);
	annaClient->subscribeTo(config::KEY_UNBATCHERS);

	proposers = new client_component(zmqNetwork, config::PROPOSER_PORT_FOR_PROXY_LEADERS, Proposer,
	                           [&](const std::string& address, const time_t now) {
		BENCHMARK_LOG("Proxy leader connected to proposer at {}", address);
		proposers->sendToIp(address, ""); //send first heartbeat
	},[](const std::string& address, const time_t now) {
		BENCHMARK_LOG("ERROR??: Proposer disconnected from proposer at {}", address);
	}, [&](const std::string& address, const std::string& payload, const time_t now) {
		ProposerToAcceptor proposerToAcceptor;
		proposerToAcceptor.ParseFromString(payload);
		listenToProposer(proposerToAcceptor, address);
	});
	proposers->startHeartbeater();

	unbatcherHeartbeat = new heartbeat_component(zmqNetwork);
	unbatchers = new client_component(zmqNetwork, config::UNBATCHER_PORT_FOR_PROXY_LEADERS, Unbatcher,
	                           [&](const std::string& address, const time_t now) {
		BENCHMARK_LOG("Proxy leader connected to unbatcher at {}", address);
		unbatcherHeartbeat->addConnection(address, now);
	},[&](const std::string& address, const time_t now) {
		BENCHMARK_LOG("Proxy leader disconnected from unbatcher at {}", address);
		unbatcherHeartbeat->removeConnection(address);
	}, [&](const std::string& address, const std::string& payload, const time_t now) {
		unbatcherHeartbeat->addHeartbeat(address, now);
	});

	zmqNetwork->poll();
}

void proxy_leader::listenToAnna(const std::string& key, const two_p_set& twoPSet, const time_t now) {
    if (key == config::KEY_PROPOSERS) {
        proposers->connectToNewMembers(twoPSet, now);
        if (proposers->numConnections() == config::F + 1)
	        annaClient->unsubscribeFrom(config::KEY_PROPOSERS);
    }
    else if (key == config::KEY_UNBATCHERS) {
        unbatchers->connectToNewMembers(twoPSet, now);
    }
    else if (key == config::KEY_ACCEPTOR_GROUPS) {
	    // merge new acceptor group IDs
	    const two_p_set& updates = acceptorGroupIdSet.updatesFrom(twoPSet);
	    if (updates.empty())
		    return;

	    for (const std::string& acceptorGroupId : updates.getObserved()) {
		    processNewAcceptorGroup(acceptorGroupId);
	    }
	    for (const std::string& acceptorGroupId : updates.getRemoved()) {
		    //TODO shut down acceptor group
	    }
	    acceptorGroupIdSet.merge(updates);
    }
    else {
	    //must be individual acceptor groups
	    acceptorGroups[key]->connectToNewMembers(twoPSet, now);
    }
}

void proxy_leader::processNewAcceptorGroup(const std::string& acceptorGroupId) {
	BENCHMARK_LOG("New acceptor group from proposer: {}", acceptorGroupId);
	acceptorGroups[acceptorGroupId] =
			new client_component(zmqNetwork, config::ACCEPTOR_PORT_FOR_PROXY_LEADERS, Acceptor,
						[&, acceptorGroupId](const std::string& address, const time_t now) {
				BENCHMARK_LOG("Proxy leader connected to acceptor of group {} at {}", acceptorGroupId, address);
				//Note: we assume that all acceptors will connect to us before failing
				if (acceptorGroups[acceptorGroupId]->numConnections() == 2*config::F+1) {
					BENCHMARK_LOG("Proxy leader fully connected to acceptor group {}", acceptorGroupId);
					connectedAcceptorGroups.emplace(acceptorGroupId);
					annaClient->unsubscribeFrom(acceptorGroupId);
				}
			}, [](const std::string& address, const time_t now) {
				BENCHMARK_LOG("ERROR??: Proxy leader disconnected from acceptor at {}", address);
			}, [&](const std::string& address, const std::string& payload, const time_t now) {
				AcceptorToProxyLeader acceptorToProxyLeader;
				acceptorToProxyLeader.ParseFromString(payload);
				listenToAcceptor(acceptorToProxyLeader);
			});

	annaClient->subscribeTo(acceptorGroupId); //find the IP addresses of acceptors in this group
}

void proxy_leader::listenToProposer(const ProposerToAcceptor& payload, const std::string& ipAddress) {
	LOG("Received from proposer: {}", payload.ShortDebugString());
	if (connectedAcceptorGroups.find(payload.acceptorgroupid()) == connectedAcceptorGroups.end()) {
		BENCHMARK_LOG("Dropping message from proposer to acceptor group {}", payload.acceptorgroupid());
		return;
	}

	TIME();
    sentMessages[payload.messageid()] = sentMetadata{payload, ipAddress}; // keep track
	acceptorGroups[payload.acceptorgroupid()]->broadcast(payload.SerializeAsString());
	TIME();
}

void proxy_leader::listenToAcceptor(const AcceptorToProxyLeader& payload) {
    LOG("Received from acceptors: {}", payload.ShortDebugString());

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
    const sentMetadata& sentValue = sentMessages[payload.messageid()];
    if (sentValue.value.ballot().ballotnum() == 0)  //p1b is arriving for a nonexistent sentValue
        return;

    if (Log::isBallotGreaterThan(payload.ballot(), sentValue.value.ballot())) {
        //yikes, the proposer got preempted
	    BENCHMARK_LOG("P1B preempted for proposer {}", payload.ballot().id());
        const ProxyLeaderToProposer& messageToProposer = message::createProxyP1B(payload.messageid(),
                                                                                 payload.acceptorgroupid(),
                                                                                 payload.ballot(), {}, {});
	    proposers->sendToIp(sentValue.proposerAddress, messageToProposer.SerializeAsString());
        sentMessages.erase(payload.messageid());
        unmergedLogs.erase(payload.messageid());
    }
    else {
        //add another log
        unmergedLogs[payload.messageid()].emplace_back(payload.log().begin(), payload.log().end());

        if (unmergedLogs[payload.messageid()].size() >= config::F + 1) {
            //we have f+1 good logs, merge them & tell the proposer
	        BENCHMARK_LOG("P1B approved for proposer {}", payload.ballot().id());
            const auto&[committedLog, uncommittedLog] = Log::mergeLogsOfAcceptorGroup(unmergedLogs[payload.messageid()]);
            const ProxyLeaderToProposer& messageToProposer = message::createProxyP1B(payload.messageid(),
																					 payload.acceptorgroupid(),
                                                                                     payload.ballot(),
                                                                                     committedLog, uncommittedLog);
	        proposers->sendToIp(sentValue.proposerAddress, messageToProposer.SerializeAsString());
            sentMessages.erase(payload.messageid());
            unmergedLogs.erase(payload.messageid());
        }
    }
}

void proxy_leader::handleP2B(const AcceptorToProxyLeader& payload) {
    const sentMetadata& sentValue = sentMessages[payload.messageid()];
    if (sentValue.proposerAddress.empty()) //p2b is arriving for a nonexistent sentValue
        return;

    if (Log::isBallotGreaterThan(payload.ballot(), sentValue.value.ballot())) {
        //yikes, the proposer got preempted
	    LOG("P2B preempted for proposer: {}, slot: {}", payload.ballot().id(), payload.slot());
        const ProxyLeaderToProposer& messageToProposer = message::createProxyP2B(payload.messageid(),
																				 payload.acceptorgroupid(),
                                                                                payload.ballot(), payload.slot());
	    proposers->sendToIp(sentValue.proposerAddress, messageToProposer.SerializeAsString());
	    sentMessages.erase(payload.messageid());
        approvedCommanders.erase(payload.messageid());
    }
    else {
        //add another approved commander
        approvedCommanders[payload.messageid()] += 1;

        if (approvedCommanders[payload.messageid()] >= config::F + 1) {
            //we have f+1 approved commanders, tell the unbatcher. No need to tell proposer
	        LOG("P2B approved for proposer: {}, slot: {}", payload.ballot().id(), payload.slot());
	        unbatchers->sendToIp(unbatcherHeartbeat->nextAddress(),
							 message::createBatchMessage(sentValue.value.client(), sentValue.value.payload())
							 .SerializeAsString());
            sentMessages.erase(payload.messageid());
            approvedCommanders.erase(payload.messageid());
	        TIME();
        }
    }
}

int main(int argc, char** argv) {
    if (argc != 1) {
        printf("Usage: ./proxy_leader\n");
        exit(0);
    }

    INIT_LOGGER();
	proxy_leader pr {};
}
