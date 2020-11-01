//
// Created by David Chu on 10/29/20.
//

#include "proxy_leader.hpp"
#include "utils/config.hpp"
#include "utils/network.hpp"
#include "models/message.hpp"
#include <thread>

proxy_leader::proxy_leader(int id) : id(id) {
    connectToProposers();
    connectToAcceptors();
    sendHeartbeat();
}

void proxy_leader::connectToProposers() {
    for (int i = 0; i < config::F + 1; i++) {
        const int proposerPort = config::PROPOSER_PORT_START + i;
        threads.emplace_back(std::thread([&, i, proposerPort]{
            const int proposerSocket = network::connectToServerAtAddress(config::LOCALHOST, proposerPort);
            network::sendPayload(proposerSocket, message::createWhoIsThis(WhoIsThis_Sender_proxyLeader));
            printf("Proxy leader %d connected to proposer\n", id);
            {std::lock_guard<std::mutex> lock(proposerMutex);
                proposerSockets[i] = proposerSocket;}
            listenToProposer(proposerSocket);
        }));
    }
}

void proxy_leader::listenToProposer(int socket) {
    ProposerToAcceptor payload;
    while (true) {
        payload.ParseFromString(network::receivePayload(socket));
        //keep track
        {std::lock_guard<std::mutex> lock(sentMessagesMutex);
            sentMessages[payload.messageid()] = payload;}
        printf("Proxy leader %d received from proposer: %s\n", id, payload.ShortDebugString().c_str());

        //broadcast to acceptors
        std::lock_guard<std::mutex> lock(acceptorMutex);
        //TODO check if connection to acceptors is valid for the ID; if not, fetch from Anna
        for (const int acceptorSocket : acceptorSockets[payload.acceptorgroupid()])
            network::sendPayload(acceptorSocket, payload);
    }
}

void proxy_leader::connectToAcceptors() {
    for (int acceptorGroupId = 0; acceptorGroupId < config::NUM_ACCEPTOR_GROUPS; acceptorGroupId++) {
        const int acceptorGroupPortOffset = config::ACCEPTOR_GROUP_PORT_OFFSET * acceptorGroupId;
        {std::lock_guard<std::mutex> lock(acceptorMutex);
            acceptorGroupIds.emplace_back(acceptorGroupId);}

        for (int i = 0; i < 2 * config::F + 1; i++) {
            const int acceptorPort = config::ACCEPTOR_PORT_START + acceptorGroupPortOffset + i;
            threads.emplace_back(std::thread([&, acceptorPort, acceptorGroupId]{
                const int acceptorSocket = network::connectToServerAtAddress(config::LOCALHOST, acceptorPort);
                {std::lock_guard<std::mutex> lock(acceptorMutex);
                    acceptorSockets[acceptorGroupId].emplace_back(acceptorSocket);}
                listenToAcceptor(acceptorSocket);
            }));
        }
    }
}

void proxy_leader::listenToAcceptor(int socket) {
    AcceptorToProxyLeader payload;
    while (true) {
        payload.ParseFromString(network::receivePayload(socket));
        printf("Proxy leader %d received from acceptors: %s\n", id, payload.ShortDebugString().c_str());

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
}

void proxy_leader::handleP1B(const AcceptorToProxyLeader& payload) {
    std::scoped_lock lock(sentMessagesMutex, unmergedLogsMutex, proposerMutex);
    const ProposerToAcceptor& sentValue = sentMessages[payload.messageid()];
    if (sentValue.ballot().ballotnum() == 0) {
        //p1b is arriving for a nonexistent sentValue
        return;
    }

    if (Log::isBallotGreaterThan(payload.ballot(), sentValue.ballot())) {
        //yikes, the proposer got preempted
        const ProxyLeaderToProposer& messageToProposer = message::createProxyP1B(payload.messageid(), payload.acceptorgroupid(),
                                                                                 payload.ballot(), {}, {});
        network::sendPayload(proposerSockets[sentValue.ballot().id()], messageToProposer);
        sentMessages.erase(payload.messageid());
        unmergedLogs.erase(payload.messageid());
    }
    else {
        //add another log
        unmergedLogs[payload.messageid()].emplace_back(payload.log().begin(), payload.log().end());

        if (unmergedLogs[payload.messageid()].size() >= config::F + 1) {
            //we have f+1 good logs, merge them & tell the proposer
            const auto&[committedLog, uncommittedLog] = Log::mergeLogsOfAcceptorGroup(unmergedLogs[payload.messageid()]);
            const ProxyLeaderToProposer& messageToProposer = message::createProxyP1B(payload.messageid(), payload.acceptorgroupid(),
                                                                                     payload.ballot(), committedLog, uncommittedLog);
            network::sendPayload(proposerSockets[sentValue.ballot().id()], messageToProposer);
            sentMessages.erase(payload.messageid());
            unmergedLogs.erase(payload.messageid());
        }
    }
}

void proxy_leader::handleP2B(const AcceptorToProxyLeader& payload) {
    std::scoped_lock lock(sentMessagesMutex, approvedCommandersMutex, proposerMutex);
    const ProposerToAcceptor& sentValue = sentMessages[payload.messageid()];
    if (sentValue.slot() == 0) {
        //p2b is arriving for a nonexistent sentValue
        return;
    }

    if (Log::isBallotGreaterThan(payload.ballot(), sentValue.ballot())) {
        //yikes, the proposer got preempted
        const ProxyLeaderToProposer& messageToProposer = message::createProxyP2B(payload.messageid(), payload.acceptorgroupid(),
                                                                                payload.ballot(), payload.slot());
        network::sendPayload(proposerSockets[sentValue.ballot().id()], messageToProposer);
        sentMessages.erase(payload.messageid());
        approvedCommanders.erase(payload.messageid());
    }
    else {
        //add another approved commander
        approvedCommanders[payload.messageid()] += 1;

        if (approvedCommanders[payload.messageid()] >= config::F + 1) {
            //we have f+1 approved commanders, tell the proposer
            const ProxyLeaderToProposer& messageToProposer = message::createProxyP2B(payload.messageid(), payload.acceptorgroupid(),
                                                                                     payload.ballot(), payload.slot());
            network::sendPayload(proposerSockets[sentValue.ballot().id()], messageToProposer);
            sentMessages.erase(payload.messageid());
            approvedCommanders.erase(payload.messageid());
        }
    }
}

[[noreturn]]
void proxy_leader::sendHeartbeat() {
    ProxyLeaderToProposer heartbeat = message::createProxyLeaderHeartbeat();
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(config::HEARTBEAT_SLEEP_SEC));
        std::lock_guard<std::mutex> lock(proposerMutex);
        for (const auto&[id, socket] : proposerSockets)
            network::sendPayload(socket, heartbeat);
    }
}
