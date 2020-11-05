//
// Created by David Chu on 10/29/20.
//

#include "proxy_leader.hpp"
#include "utils/config.hpp"
#include "utils/network.hpp"
#include "models/message.hpp"
#include <thread>

proxy_leader::proxy_leader(int id, std::map<int, std::string> proposers, std::map<int, std::map<int, std::string>> acceptors) : id(id) {
    connectToProposers(proposers);
    connectToAcceptors(acceptors);
    sendHeartbeat();
}

void proxy_leader::connectToProposers(std::map<int, std::string> proposers) {
    for (const auto pair : proposers) {
        int i = pair.first;
        std::string proposer_address = pair.second;
        threads.emplace_back(std::thread([&, i, proposer_address]{
            const int proposerSocket = network::connectToServerAtAddress(proposer_address, config::PROPOSER_PORT_START + i);
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
        const std::optional<std::string>& incoming = network::receivePayload(socket);
        if (incoming->empty())
            return;
        payload.ParseFromString(incoming.value());
        //keep track
        {std::lock_guard<std::mutex> lock(sentMessagesMutex);
            sentMessages[payload.messageid()] = payload;}
        printf("Proxy leader %d received from proposer: %s\n", id, payload.ShortDebugString().c_str());

        //broadcast to acceptors
        std::lock_guard<std::mutex> lock(acceptorMutex);
        //TODO check if connection to acceptors is valid for the ID; if not, fetch from Anna
        for (const int acceptorSocket : acceptorSockets[payload.acceptorgroupid()])
            network::sendPayload(acceptorSocket, payload);

        payload.Clear();
    }
}

void proxy_leader::connectToAcceptors(std::map<int, std::map<int, std::string>> acceptors) {
    for (const auto pair : acceptors) {
        int acceptorGroupId = pair.first;
        std::map<int, std::string> acceptorIPs = pair.second;
        const int acceptorGroupPortOffset = config::ACCEPTOR_GROUP_PORT_OFFSET * acceptorGroupId;
        {std::lock_guard<std::mutex> lock(acceptorMutex);
            acceptorGroupIds.emplace_back(acceptorGroupId);}

        for (const auto acceptor: acceptorIPs) {
            int i = acceptor.first;
            std::string acceptor_ip = acceptor.second;
            const int acceptorPort = config::ACCEPTOR_PORT_START + acceptorGroupPortOffset + i;
            threads.emplace_back(std::thread([&, acceptor_ip, acceptorPort, acceptorGroupId]{
                const int acceptorSocket = network::connectToServerAtAddress(acceptor_ip, acceptorPort);
                printf("Proxy leader %d connected to acceptor\n", id);
                {std::lock_guard<std::mutex> lock(acceptorMutex);
                    acceptorSockets[acceptorGroupId].emplace_back(acceptorSocket);}
                listenToAcceptor(acceptorSocket);
                printf("Proxy leader %d disconnected from acceptor!!!\n", id);
            }));
        }
    }
}

void proxy_leader::listenToAcceptor(int socket) {
    AcceptorToProxyLeader payload;
    while (true) {
        const std::optional<std::string>& incoming = network::receivePayload(socket);
        if (incoming->empty())
            return;
        payload.ParseFromString(incoming.value());
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
        payload.Clear();
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

int main(int argc, char** argv) {
    if(argc != 4) {
        printf("Please follow the format for running this function: ./proxy_leader <PROXY LEADER ID> <PROPOSER FILE NAME> <ACCEPTOR FILE NAME>.\n");
        exit(0);
    }
    int proxy_leader_id = atoi( argv[1] );
    std::string proposer_file = argv[2];
    std::map<int, std::string> proposers = parser::parse_proposer(proposer_file);
    std::string acceptor_file = argv[3];
    std::map<int, std::map<int, std::string>> acceptors = parser::parse_acceptors(acceptor_file);
    proxy_leader main = proxy_leader(proxy_leader_id, proposers, acceptors);
}
