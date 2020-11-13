//
// Created by David Chu on 10/29/20.
//

#include "proxy_leader.hpp"
#include "utils/config.hpp"
#include "utils/network.hpp"
#include "utils/heartbeater.hpp"
#include "models/message.hpp"
#include <thread>

proxy_leader::proxy_leader(const int id, const parser::idToIP& unbatchersIdToIps, const parser::idToIP& proposers,
                           const std::unordered_map<int, parser::idToIP>& acceptors) : id(id), unbatchers(config::F+1) {
    connectToUnbatchers(unbatchersIdToIps);
    connectToProposers(proposers);
    connectToAcceptors(acceptors);
    heartbeater::heartbeat(message::createProxyLeaderHeartbeat(), proposerMutex, proposerSockets);
    pthread_exit(nullptr);
}

void proxy_leader::connectToUnbatchers(const parser::idToIP& unbatchersIdToIps) {
    unbatchers.connectToServers(unbatchersIdToIps, config::UNBATCHER_PORT_START, WhoIsThis_Sender_proxyLeader,
        [&](const int socket, const std::string& payload) {
        unbatchers.addHeartbeat(socket);
    });
    unbatchers.waitForThreshold();
}

void proxy_leader::connectToProposers(const parser::idToIP& proposers) {
    for (const auto& proposerIdToIps : proposers) {
        const int proposerId = proposerIdToIps.first;
        const std::string& proposerIp = proposerIdToIps.second;

        std::thread thread([&, proposerId, proposerIp] {
            const int socket = network::connectToServerAtAddress(proposerIp, config::PROPOSER_PORT_START + proposerId, WhoIsThis_Sender_proxyLeader);
            printf("Proxy leader %d connected to proposer\n", id);

            std::unique_lock lock(proposerMutex);
            proposerSockets[proposerId] = socket;
            lock.unlock();

            network::listenToSocketUntilClose(socket, [&](const int socket, const std::string& payloadString) {
                ProposerToAcceptor payload;
                payload.ParseFromString(payloadString);
                listenToProposer(payload);
            });
        });
        thread.detach();
    }
}

void proxy_leader::listenToProposer(const ProposerToAcceptor& payload) {
    // keep track
    std::unique_lock messagesLock(sentMessagesMutex);
    sentMessages[payload.messageid()] = payload;
    messagesLock.unlock();
    printf("Proxy leader %d received from proposer: %s\n", id, payload.ShortDebugString().c_str());

    // Broadcast to Acceptors; wait if necessary
    std::shared_lock acceptorsLock(acceptorMutex);
    acceptorCV.wait(acceptorsLock, [&]{return acceptorSockets.find(payload.acceptorgroupid()) != acceptorSockets.end();});
    // TODO check if connection to acceptors is valid for the ID; if not, fetch from Anna
    for (const int acceptorSocket : acceptorSockets[payload.acceptorgroupid()])
        network::sendPayload(acceptorSocket, payload);
}

void proxy_leader::connectToAcceptors(const std::unordered_map<int, parser::idToIP>& acceptors) {
    for (const auto& acceptorGroupIdtoMemberIps : acceptors) {
        const int acceptorGroupId = acceptorGroupIdtoMemberIps.first;
        const parser::idToIP& memberIdtoIps = acceptorGroupIdtoMemberIps.second;
        const int acceptorGroupPortOffset = config::ACCEPTOR_GROUP_PORT_OFFSET * acceptorGroupId;
        std::unique_lock acceptorsLock1(acceptorMutex);
        acceptorGroupIds.emplace_back(acceptorGroupId);
        acceptorsLock1.unlock();

        for (const auto& acceptor: memberIdtoIps) {
            const int acceptorId = acceptor.first;
            const std::string& acceptorIp = acceptor.second;
            const int acceptorPort = config::ACCEPTOR_PORT_START + acceptorGroupPortOffset + acceptorId;

            std::thread thread([&, acceptorIp, acceptorPort, acceptorGroupId]{
                const int socket = network::connectToServerAtAddress(acceptorIp, acceptorPort, WhoIsThis_Sender_proxyLeader);
                printf("Proxy leader %d connected to acceptor\n", id);
                std::unique_lock acceptorsLock2(acceptorMutex);
                acceptorSockets[acceptorGroupId].emplace_back(socket);
                if (acceptorSockets[acceptorGroupId].size() >= 2 * config::F + 1) {
                    acceptorsLock2.unlock();
                    acceptorCV.notify_one();
                }
                else
                    acceptorsLock2.unlock();

                network::listenToSocketUntilClose(socket, [&](const int socket, const std::string& payloadString) {
                    AcceptorToProxyLeader payload;
                    payload.ParseFromString(payloadString);
                    listenToAcceptor(payload);
                });
            });
            thread.detach();
        }
    }
}

void proxy_leader::listenToAcceptor(const AcceptorToProxyLeader& payload) {
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

void proxy_leader::handleP1B(const AcceptorToProxyLeader& payload) {
    std::shared_lock messagesLock(sentMessagesMutex);
    const ProposerToAcceptor& sentValue = sentMessages[payload.messageid()];
    if (sentValue.ballot().ballotnum() == 0) {
        //p1b is arriving for a nonexistent sentValue
        return;
    }
    messagesLock.unlock();

    std::shared_lock proposerLock(proposerMutex, std::defer_lock);
    std::scoped_lock lock(sentMessagesMutex, unmergedLogsMutex, proposerLock);
    if (Log::isBallotGreaterThan(payload.ballot(), sentValue.ballot())) {
        //yikes, the proposer got preempted
        const ProxyLeaderToProposer& messageToProposer = message::createProxyP1B(payload.messageid(),
                                                                                 payload.acceptorgroupid(),
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
    std::shared_lock messagesLock(sentMessagesMutex);
    const ProposerToAcceptor& sentValue = sentMessages[payload.messageid()];
    if (sentValue.slot() == 0) {
        //p2b is arriving for a nonexistent sentValue
        return;
    }
    messagesLock.unlock();

    std::shared_lock proposerLock(proposerMutex, std::defer_lock);
    std::scoped_lock lock(sentMessagesMutex, approvedCommandersMutex, proposerLock);
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
            unbatchers.send(sentMessages[payload.messageid()].payload());
            sentMessages.erase(payload.messageid());
            approvedCommanders.erase(payload.messageid());
        }
    }
}

int main(int argc, char** argv) {
    if (argc != 5) {
        printf("Usage: ./proxy_leader <PROXY LEADER ID> <UNBATCHER FILE NAME> <PROPOSER FILE NAME> <ACCEPTOR FILE NAME>.\n");
        exit(0);
    }
    const int id = atoi(argv[1]);
    const std::string& unbatcherFileName = argv[2];
    const parser::idToIP& unbatchers = parser::parseIDtoIPs(unbatcherFileName);
    const std::string& proposerFileName = argv[3];
    const parser::idToIP& proposers = parser::parseIDtoIPs(proposerFileName);
    const std::string& acceptorFileName = argv[4];
    const std::unordered_map<int, parser::idToIP>& acceptors = parser::parseAcceptors(acceptorFileName);
    proxy_leader(id, unbatchers, proposers, acceptors);
}
