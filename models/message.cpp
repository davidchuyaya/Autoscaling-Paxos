//
// Created by David Chu on 10/6/20.
//

#include "message.hpp"

WhoIsThis message::createWhoIsThis(const WhoIsThis_Sender& sender) {
    WhoIsThis whoIsThis;
    whoIsThis.set_sender(sender);
    return whoIsThis;
}

ProposerToAcceptor message::createP1A(const int id, const int ballotNum, const std::string& acceptorGroupId,
									  const std::string& ipAddress) {
    ProposerToAcceptor p1a;
    p1a.set_messageid(uuid::generate());
    p1a.set_type(ProposerToAcceptor_Type_p1a);
    p1a.set_acceptorgroupid(acceptorGroupId);
	p1a.set_ipaddress(ipAddress);
    Ballot* ballot = p1a.mutable_ballot();
    ballot->set_id(id);
    ballot->set_ballotnum(ballotNum);
    return p1a;
}

AcceptorToProxyLeader
message::createP1B(const int messageId, const std::string& acceptorGroupId, const Ballot& highestBallot,
                   const Log::pValueLog& log) {
    AcceptorToProxyLeader p1b;
    p1b.set_messageid(messageId);
    p1b.set_type(AcceptorToProxyLeader_Type_p1b);
    p1b.set_acceptorgroupid(acceptorGroupId);
    *p1b.mutable_ballot() = highestBallot;
    *p1b.mutable_log() = {log.begin(), log.end()};
    return p1b;
}

ProposerToAcceptor message::createP2A(const int id, const int ballotNum, const int slot, const std::string& payload,
                                      const std::string& acceptorGroupId, const std::string& ipAddress) {
    ProposerToAcceptor p2a;
    p2a.set_messageid(uuid::generate());
    p2a.set_type(ProposerToAcceptor_Type_p2a);
    Ballot* ballot = p2a.mutable_ballot();
    ballot->set_id(id);
    ballot->set_ballotnum(ballotNum);
    p2a.set_slot(slot);
    p2a.set_payload(payload);
    p2a.set_acceptorgroupid(acceptorGroupId);
	p2a.set_ipaddress(ipAddress);
    return p2a;
}

AcceptorToProxyLeader message::createP2B(const int messageId, const std::string& acceptorGroupId,
                                         const Ballot& highestBallot, const int slot) {
    AcceptorToProxyLeader p2b;
    p2b.set_messageid(messageId);
    p2b.set_type(AcceptorToProxyLeader_Type_p2b);
    p2b.set_acceptorgroupid(acceptorGroupId);
    *p2b.mutable_ballot() = highestBallot;
    p2b.set_slot(slot);
    return p2b;
}

ProxyLeaderToProposer message::createProxyP1B(const int messageId, const std::string& acceptorGroupId,
                                              const Ballot& highestBallot, const Log::stringLog& committedLog,
                                              const Log::pValueLog& uncommittedLog) {
    ProxyLeaderToProposer p1b;
    p1b.set_messageid(messageId);
    p1b.set_type(ProxyLeaderToProposer_Type_p1b);
    p1b.set_acceptorgroupid(acceptorGroupId);
    *p1b.mutable_ballot() = highestBallot;
    *p1b.mutable_committedlog() = {committedLog.begin(), committedLog.end()};
    *p1b.mutable_uncommittedlog() = {uncommittedLog.begin(), uncommittedLog.end()};
    return p1b;
}

ProxyLeaderToProposer message::createProxyP2B(const int messageId, const std::string& acceptorGroupId,
                                              const Ballot& highestBallot, const int slot) {
    ProxyLeaderToProposer p2b;
    p2b.set_messageid(messageId);
    p2b.set_type(ProxyLeaderToProposer_Type_p2b);
    p2b.set_acceptorgroupid(acceptorGroupId);
    *p2b.mutable_ballot() = highestBallot;
    p2b.set_slot(slot);
    return p2b;
}

ProxyLeaderToProposer message::createProxyLeaderHeartbeat() {
    ProxyLeaderToProposer heartbeat;
    heartbeat.set_type(ProxyLeaderToProposer_Type_heartbeat);
    return heartbeat;
}

Heartbeat message::createGenericHeartbeat() {
	Heartbeat heartbeat;
	heartbeat.set_dummy(true); //must have at least 1 value for message to send
	return heartbeat;
}

ProposerToProposer message::createIamLeader() {
    ProposerToProposer iAmLeader;
    iAmLeader.set_iamleader(true);
    return iAmLeader;
}

ClientToBatcher message::createClientRequest(const std::string& ipAddress, const std::string& payload) {
    ClientToBatcher clientToBatcher;
    clientToBatcher.set_ipaddress(ipAddress);
    clientToBatcher.set_request(payload);
    return clientToBatcher;
}

Batch message::createBatchMessage(const std::unordered_map<std::string, std::string>& requests) {
    Batch batch;
    *batch.mutable_clienttorequest() = {requests.begin(), requests.end()};
    return batch;
}

UnbatcherToClient message::createUnbatcherToClientAck(const std::string& request) {
	UnbatcherToClient unbatcherToClient;
	unbatcherToClient.set_request(request);
	return unbatcherToClient;
}
