//
// Created by David Chu on 10/4/20.
//
#include "proposer.hpp"

proposer::proposer(const int id, const int numAcceptorGroups) : id(id), numAcceptorGroups(numAcceptorGroups) {
//    annaClient = anna::readWritable({{config::KEY_PROPOSERS, config::IP_ADDRESS}},
//                    [&](const std::string& key, const two_p_set& twoPSet) {
//    	listenToAnna(key, twoPSet);
//    });
//	annaClient->subscribeTo(config::KEY_PROPOSERS);
//	annaClient->subscribeTo(config::KEY_ACCEPTOR_GROUPS);

    //wait for acceptor group IDs before starting phase 1 or phase 2. All batches will be dropped until then.
//    acceptorCV.wait(lock, [&]{ return acceptorGroupIds.size() >= numAcceptorGroups; });
//    BENCHMARK_LOG("Acceptor group threshold met\n");
	zmqNetwork = new network();

	proposers = new client_component(zmqNetwork, config::PROPOSER_PORT_FOR_PROPOSERS, Proposer,
							[](const std::string& address, const time_t now) {
		BENCHMARK_LOG("Proposer connected to proposer at {}", address);
	},[](const std::string& address, const time_t now) {
		BENCHMARK_LOG("ERROR??: Proposer disconnected from proposer at {}", address);
	}, [&](const std::string& address, const std::string& payload, const time_t now) {
		listenToProposer();
	});
	proposers->connectToNewMembers({{"54.219.37.153", "13.52.215.70"},{}}, 0); //TODO add new members with anna
	//we don't talk to other proposers through the server port
	zmqNetwork->startServerAtPort(config::PROPOSER_PORT_FOR_PROPOSERS, Proposer);

	proxyLeaderHeartbeat = new heartbeat_component(zmqNetwork);
	proxyLeaders = new server_component(zmqNetwork, config::PROPOSER_PORT_FOR_PROXY_LEADERS, ProxyLeader,
	                              [&](const std::string& address, const time_t now) {
		BENCHMARK_LOG("Proxy leader from {} connected to proposer", address);
		proxyLeaderHeartbeat->addConnection(address, now);
	}, [&](const std::string& address, const std::string& payload, const time_t now) {
		proxyLeaderHeartbeat->addHeartbeat(address, now);
		if (payload.empty()) //just a heartbeat
			return;
		ProxyLeaderToProposer proxyLeaderToProposer;
		proxyLeaderToProposer.ParseFromString(payload);
		listenToProxyLeader(proxyLeaderToProposer);
		proxyLeaderToProposer.Clear();
	});

	batchers = new server_component(zmqNetwork, config::PROPOSER_PORT_FOR_BATCHERS, Batcher,
						   [](const std::string& address, const time_t now) {
		BENCHMARK_LOG("Batcher from {} connected to proposer", address);
	}, [&](const std::string& address, const std::string& payload, const time_t now) {
		Batch batch;
		batch.ParseFromString(payload);
		listenToBatcher(batch);
		batch.Clear();
	});

	acceptorGroupIds.emplace_back("1"); //TODO add new acceptor groups with anna

	//leader loop
	zmqNetwork->addTimer([&](const time_t now) {
		//send heartbeats
		if (isLeader) {
			LOG("I am leader, sending at time: {}", std::asctime(std::localtime(&now)));
			proposers->broadcast("");
		}
		//ID-based timeout so a leader doesn't have much competition
		else if (difftime(now, lastLeaderHeartbeat) > config::HEARTBEAT_TIMEOUT_SEC + id * config::ID_SCOUT_DELAY_MULTIPLIER) {
			sendScouts();
		}
		BENCHMARK_LOG("Processed {} messages", nextSlot);
	}, config::HEARTBEAT_SLEEP_SEC, true);

	zmqNetwork->poll();
}

void proposer::listenToAnna(const std::string& key, const two_p_set& twoPSet) {
//    if (key == config::KEY_PROPOSERS) {
//        // connect to new proposer
//        proposers.connectAndMaybeListen(twoPSet);
//        if (proposers.twoPsetThresholdMet())
//	        annaClient->unsubscribeFrom(config::KEY_PROPOSERS);
//    } else if (key == config::KEY_ACCEPTOR_GROUPS) {
//        // merge new acceptor group IDs
//        const two_p_set& updates = acceptorGroupIdSet.updatesFrom(twoPSet);
//        if (updates.empty())
//	        return;
//
//        for (const std::string& acceptorGroupId : updates.getObserved()) {
//            acceptorGroupIds.emplace_back(acceptorGroupId);
//            //TODO if leader, attempt to win matchmakers with new configuration
//        }
//        for (const std::string& acceptorGroupId : updates.getRemoved()) {
//            acceptorGroupIds.erase(std::remove(acceptorGroupIds.begin(), acceptorGroupIds.end(), acceptorGroupId), acceptorGroupIds.end());
//        }
//        acceptorGroupIdSet.merge(updates);
//
//        //awaken main thread if we're past the threshold
//        if (acceptorGroupIds.size() >= numAcceptorGroups) {
//	        annaClient->unsubscribeFrom(config::KEY_ACCEPTOR_GROUPS);
//        }
//    }
}

void proposer::listenToBatcher(const Batch& payload) {
    LOG("Received batch request: {}", payload.ShortDebugString());
    if (!isLeader)
        return;

	TIME();
    int slot;
    if (logHoles.empty()) {
        slot = nextSlot;
        nextSlot += 1;
    }
    else {
        slot = logHoles.front();
        logHoles.pop();
    }
    proxyLeaders->sendToIp(proxyLeaderHeartbeat->nextAddress(),
						  message::createP2A(id, ballotNum, slot, payload.client(), payload.request(),
						   fetchNextAcceptorGroupId()).SerializeAsString());
	TIME();
}

void proposer::listenToProxyLeader(const ProxyLeaderToProposer& payload) {
    switch (payload.type()) {
        case ProxyLeaderToProposer_Type_p1b:
            handleP1B(payload);
            break;
        case ProxyLeaderToProposer_Type_p2b:
            handleP2B(payload);
            break;
        default: {}
    }
}

void proposer::listenToProposer() {
    LOG("Received leader heartbeat for time: {}", std::asctime(std::localtime(&lastLeaderHeartbeat)));
    time(&lastLeaderHeartbeat); // store the time we received the heartbeat
    noLongerLeader();
}

void proposer::sendScouts() {
	if (acceptorGroupIds.size() < numAcceptorGroups) {
		BENCHMARK_LOG("Not sending scouts because we don't have all acceptor groups yet");
		return;
	}
	noLongerLeader();

    int currentBallotNum;
    ballotNum += 1;
    currentBallotNum = ballotNum;
    BENCHMARK_LOG("P1A blasting out: id = {}, ballotNum = {}", id, currentBallotNum);

    for (const std::string& acceptorGroupId : acceptorGroupIds) {
        remainingAcceptorGroupsForScouts.emplace(acceptorGroupId);
        proxyLeaders->sendToIp(proxyLeaderHeartbeat->nextAddress(),
							  message::createP1A(id, currentBallotNum, acceptorGroupId).SerializeAsString());
    }
}

void proposer::handleP1B(const ProxyLeaderToProposer& message) {
    BENCHMARK_LOG("Received p1b: {}", message.ShortDebugString());

    if (message.ballot().id() != id) { // we lost the election
        // store the largest ballot we last saw so we can immediately catch up
        ballotNum = message.ballot().ballotnum();
        noLongerLeader();
        return;
    }

    acceptorGroupCommittedLogs.emplace_back(message.committedlog().begin(), message.committedlog().end());
    acceptorGroupUncommittedLogs[message.acceptorgroupid()] =
            {message.uncommittedlog().begin(), message.uncommittedlog().end()};
    remainingAcceptorGroupsForScouts.erase(message.acceptorgroupid());

    if (!remainingAcceptorGroupsForScouts.empty()) //we're still waiting for other acceptor groups to respond
        return;

    //leader election complete
    isLeader = true;
    LOG("I am leader!");
    proposers->broadcast("");

    mergeLogs();
}

void proposer::mergeLogs() {
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
        proxyLeaders->sendToIp(proxyLeaderHeartbeat->nextAddress(),
							  message::createP2A(id, ballotNum, slot, pValue.client(), pValue.payload(),
							acceptorGroupForSlot.at(slot)).SerializeAsString());
}

void proposer::handleP2B(const ProxyLeaderToProposer& message) {
    LOG("Received p2b: {}", message.ShortDebugString());
    if (message.ballot().id() != id) //yikes, we got preempted
        noLongerLeader();
}

void proposer::noLongerLeader() {
    isLeader = false;
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
        printf("Usage: ./proposer <PROPOSER ID> <NUM ACCEPTOR GROUPS>\n");
        exit(0);
    }

    INIT_LOGGER();

	const int id = std::stoi(argv[1]);
	const int numAcceptorGroups = std::stoi(argv[2]);
    proposer p(id, numAcceptorGroups);
}

