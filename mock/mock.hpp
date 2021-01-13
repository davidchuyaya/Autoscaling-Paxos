//
// Created by David Chu on 1/11/21.
//

#ifndef AUTOSCALING_PAXOS_MOCK_HPP
#define AUTOSCALING_PAXOS_MOCK_HPP

#include <string>
#include <memory>
#include "message.pb.h"
#include "../utils/metrics.hpp"
#include "../utils/network.hpp"
#include "../utils/config.hpp"
#include "../utils/component_types.hpp"
#include "../models/message.hpp"
#include "../lib/storage/anna.hpp"

class mock {
public:
	mock(bool isSender, const std::string& serverAddress = "");

	[[noreturn]] void client();
	[[noreturn]] void batcher();
	[[noreturn]] void proposer(const std::string& acceptorGroupId = "");
	[[noreturn]] void proxyLeaderForProposer(const std::string& acceptorGroupId);
	[[noreturn]] void proxyLeaderForAcceptor();
	[[noreturn]] void proxyLeaderForUnbatcher(const std::string& destAddress);
	[[noreturn]] void acceptor(const std::string& acceptorGroupId);
	[[noreturn]] void unbatcher();
private:
	std::shared_ptr<metrics::variables> metricsVars;
	network* zmqNetwork;
	anna* annaClient;

	const bool isSender;
	const std::string serverAddress;
	std::string clientAddress;
	int counter; //used as message payload to generate unique messages
	std::shared_ptr<socketInfo> extraSocket; //used to keep a reference to a socket in lambdas. Careful when using

	void incrementMetricsCounter();

	[[noreturn]] void genericSender(ComponentType type, int port);
	[[noreturn]] void genericReceiver(ComponentType type, int port, bool heartbeat);
	[[noreturn]] void customSender(ComponentType type, int port, const std::function<std::string()>& generateMessage);
	[[noreturn]] void customReceiver(ComponentType type, int port, bool heartbeat, const network::messageHandler& onReceive);
	std::string generateBatch(const std::string& ip = "u.nu/davidchu"); //default to my personal website
	std::string generateP2A(const std::string& acceptorGroupId = "coolAcceptorGroupId");
};


#endif //AUTOSCALING_PAXOS_MOCK_HPP
