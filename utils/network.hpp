//
// Created by David Chu on 10/4/20.
//

#ifndef C__PAXOS_NETWORKNODE_HPP
#define C__PAXOS_NETWORKNODE_HPP

#include <unistd.h>
#include <functional>
#include <utility>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <queue>
#include "config.hpp"
#include "zmq.hpp"
#include "component_types.hpp"

struct socketInfo {
public:
	zmq::socket_t socket;
	const ComponentType type;
	const bool isServer;
	const std::string senderAddress; //not set if this is a server socket

	static socketInfo serverSocket(zmq::context_t& context, const ComponentType type){
		return socketInfo(context, ZMQ_ROUTER, type, true);
	}
	static socketInfo clientSocket(zmq::context_t& context, const ComponentType type, const std::string& senderAddress){
		return socketInfo(context, ZMQ_DEALER, type, false, senderAddress);
	}
	static socketInfo customSocket(zmq::context_t& context, const int socketType, const ComponentType type) {
		return socketInfo(context, socketType, type, false);
	}
private:
	socketInfo(zmq::context_t& context, int socketType, const ComponentType type, const bool isServer,
			std::string senderAddress = "") :
		socket(context, socketType), type(type), isServer(isServer), senderAddress(std::move(senderAddress)) {}
};

class network {
public:
	using timer = std::function<void(const time_t now)>;
	using messageHandler = std::function<void(const std::string& ipAddress, const std::string& payload,
			const time_t now)>;

    network();
    [[noreturn]] void poll();
	std::shared_ptr<socketInfo> startServerAtPort(int port, ComponentType clientType);
	std::shared_ptr<socketInfo> connectToAddress(const std::string& address, int port, ComponentType serverType);
	std::shared_ptr<socketInfo> startAnnaReader(int port, ComponentType readerType);
	[[nodiscard]] std::shared_ptr<socketInfo> startAnnaWriter(const std::string& address);
	void connectExistingSocketToAddress(const std::shared_ptr<socketInfo>& client, const std::string& address);
    void sendToServer(zmq::socket_t& socket, const std::string& payload);
	void sendToClient(zmq::socket_t& socket, const std::string& clientAddress, const std::string& payload);
	void closeSocket(const std::shared_ptr<socketInfo>& socket);
	void addHandler(ComponentType sender, const messageHandler& handler);
	void addTimer(const network::timer& func, int secondsInterval, bool repeating);
	void addTimer(const timer& func, time_t now, int secondsInterval, bool repeating);
	std::string zmqMessageToString(const zmq::message_t& message);
private:
	struct timerInfo {
		timer function;
		time_t expiry;
		int secondsInterval;
		bool repeating;
	};
	class timerInfoComparator {
	public:
		bool operator() (const timerInfo& lhs, const timerInfo& rhs) const {
			return lhs.expiry > rhs.expiry; //smallest expiry first
		}
	};

	zmq::context_t context;
	std::vector<std::shared_ptr<socketInfo>> sockets;
	std::vector<zmq::pollitem_t> pollItems;
	std::unordered_set<std::shared_ptr<socketInfo>> socketsToRemove;

	std::unordered_map<ComponentType, messageHandler> handlers;
	std::priority_queue<timerInfo, std::vector<timerInfo>, timerInfoComparator> timers;

	zmq::message_t stringToZmqMessage(const std::string& s);
	void removeClosedSockets();
	void checkTimers(time_t now);
};

#endif //C__PAXOS_NETWORKNODE_HPP
