//
// Created by David Chu on 10/6/20.
//

#include "message.hpp"

ProposerToAcceptor message::createP1A(const int id, const int ballotNum, const std::string& acceptorGroupId) {
    ProposerToAcceptor p1a;
    p1a.set_messageid(uuid::generate());
    p1a.set_type(ProposerToAcceptor_Type_p1a);
    p1a.set_acceptorgroupid(acceptorGroupId);
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

ProposerToAcceptor message::createP2A(const int id, const int ballotNum, const int slot, const std::string& client,
									  const std::string& payload, const std::string& acceptorGroupId) {
    ProposerToAcceptor p2a;
    p2a.set_messageid(uuid::generate());
    p2a.set_type(ProposerToAcceptor_Type_p2a);
    Ballot* ballot = p2a.mutable_ballot();
    ballot->set_id(id);
    ballot->set_ballotnum(ballotNum);
    p2a.set_slot(slot);
    p2a.set_client(client);
    p2a.set_payload(payload);
    p2a.set_acceptorgroupid(acceptorGroupId);
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

Batch message::createBatchMessage(const std::string& ipAddress, const std::string& requests) {
	Batch batch;
	batch.set_client(ipAddress);
	batch.set_request(requests);
	return batch;
}

KeyRequest message::createAnnaPutRequest(const std::string& prefixedKey, const std::string& payload) {
	KeyRequest request;
	request.set_request_id(std::to_string(uuid::generate()));
	request.set_response_address("tcp://" + config::IP_ADDRESS + ":" + std::to_string(config::ANNA_RESPONSE_PORT));
	request.set_type(RequestType::PUT);

	KeyTuple* tuple = request.add_tuples();
	tuple->set_key(prefixedKey);
	tuple->set_lattice_type(SET);
	tuple->set_payload(payload);
	return request;
}

KeyRequest message::createAnnaGetRequest(const std::string& prefixedKey) {
	KeyRequest request;
	request.set_request_id(std::to_string(uuid::generate()));
	request.set_response_address("tcp://" + config::IP_ADDRESS + ":" + std::to_string(config::ANNA_RESPONSE_PORT));
	request.set_type(RequestType::GET);

	KeyTuple* tuple = request.add_tuples();
	tuple->set_key(prefixedKey);
	return request;
}

KeyAddressRequest message::createAnnaKeyAddressRequest(const std::string& prefixedKey) {
	KeyAddressRequest request;
	request.set_request_id(std::to_string(uuid::generate()));
	request.set_response_address("tcp://" + config::IP_ADDRESS + ":" +
		std::to_string(config::ANNA_KEY_ADDRESS_PORT));
	request.add_keys(prefixedKey);
	return request;
}

SetValue message::createAnnaSet(const std::unordered_set<std::string>& set) {
	SetValue setValue;
	*setValue.mutable_values() = {set.begin(), set.end()};
	return setValue;
}