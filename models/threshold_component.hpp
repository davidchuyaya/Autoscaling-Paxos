//
// Created by David Chu on 11/19/20.
//

#ifndef AUTOSCALING_PAXOS_THRESHOLD_COMPONENT_HPP
#define AUTOSCALING_PAXOS_THRESHOLD_COMPONENT_HPP

#include <shared_mutex>
#include <condition_variable>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <thread>
#include <functional>
#include <algorithm>
#include "../lib/storage/two_p_set.hpp"
#include "message.pb.h"
#include "../utils/network.hpp"
#include "message.hpp"

template<typename SendMessage, typename ReceiveMessage>
class threshold_component {
public:
	//Note: Must provide port & whoIsThis if this is connecting to a server (you will use connectAndMaybeListen)
	//Note: Must provide listener if they will send you messages
    explicit threshold_component(const int waitThreshold, const int port = 0,
								 const WhoIsThis_Sender whoIsThis = WhoIsThis_Sender_null,
								 const std::optional<std::function<void(int, const ReceiveMessage&)>>& listener = {}) :
								 waitThreshold(waitThreshold), port(port), whoIsThis(whoIsThis), listener(listener) {}

    void connectAndMaybeListen(const two_p_set& newMembers) {
		std::unique_lock membersLock(membersMutex);
		const two_p_set& updates = members.updatesFrom(newMembers);
		if (updates.empty())
			return;
		members.merge(updates);
		membersLock.unlock();

		for (const std::string& ip : updates.getObserved())
			connectToIp(ip);

		if (!updates.getRemoved().empty()) {
			std::scoped_lock lock(ipToSocketMutex, componentMutex);
			for (const std::string& ip : updates.getRemoved())
				disconnectFromIp(ip);
		}
	}
	void connectToIp(const std::string& ipAddress) {
		if (ipAddress == config::IP_ADDRESS) //Don't connect to yourself
			return;

		LOG("Connecting to new member: {}\n", ipAddress);
		std::thread thread([&, ipAddress]{
			const int socket = network::connectToServerAtAddress(ipAddress, port, whoIsThis);
			std::unique_lock lock(ipToSocketMutex);
			ipToSocket[ipAddress] = socket;
			lock.unlock();
			addConnection(socket);
			if (listener.has_value())
				network::listenToSocketUntilClose(socket, listener.value());
		});
		thread.detach();
	}
    virtual void addConnection(int socket) {
	    std::unique_lock lock(componentMutex);
	    components.emplace_back(socket);
	    //check threshold
	    if (!thresholdMet())
		    return;
	    lock.unlock();
	    componentCV.notify_all();
    }
    void addSelfAsConnection() {
	    addedSelfAsConnection = true;
    }
	void removeConnection(const int socketToRemove) {
		for (const auto&[ip, socket] : ipToSocket) {
			if (socket == socketToRemove) {
				disconnectFromIp(ip);
				return;
			}
		}
    }
	virtual void disconnectFromIp(const std::string& ipAddress) {
		LOG("Removing dead member: {}\n", ipAddress);
		const int socket = ipToSocket[ipAddress];
		shutdown(socket, 1);
		components.erase(std::remove(components.begin(), components.end(), socket), components.end());
		ipToSocket.erase(ipAddress);
    }
    void broadcast(const SendMessage& payload) {
        std::shared_lock lock(componentMutex);
        if (!canSend) //block if not enough connections
            waitForThreshold(lock);
        for (int socket : components) {
	        bool success = network::sendPayload(socket, payload);

	        if (!success) { //remove broken socket
	        	lock.unlock();
		        {
			        std::scoped_lock lock2(ipToSocketMutex, componentMutex);
			        removeConnection(socket);
		        }
	        	lock.lock();
	        }
        }
    }
    void sendToIp(const std::string& ipAddress, const SendMessage& payload) {
		std::shared_lock lock(ipToSocketMutex);
		const int socket = ipToSocket.at(ipAddress);
		bool success = network::sendPayload(socket, payload);
		if (!success) {
			ipToSocketMutex.unlock();
			std::scoped_lock lock2(ipToSocketMutex, componentMutex);
			disconnectFromIp(ipAddress);
		}
    }
	[[nodiscard]] bool twoPsetThresholdMet() {
		std::shared_lock lock(membersMutex);
		return members.getObserved().size() >= waitThreshold;
    }
protected:
    const int waitThreshold;
    const int port;
    const WhoIsThis_Sender whoIsThis;
	const std::optional<std::function<void(int, const ReceiveMessage&)>>& listener;

    std::shared_mutex membersMutex;
    two_p_set members;

    std::shared_mutex ipToSocketMutex;
    std::unordered_map<std::string, int> ipToSocket = {};

    std::shared_mutex componentMutex;
    std::condition_variable_any componentCV;
    std::vector<int> components = {};
    bool canSend = false;
    bool addedSelfAsConnection = false;

    void waitForThreshold(std::shared_lock<std::shared_mutex>& lock) {
	    componentCV.wait(lock, [&]{return thresholdMet();});
	    canSend = true;
    }
    virtual bool thresholdMet() const {
	    return components.size() + (addedSelfAsConnection ? 1 : 0) >= waitThreshold;
    }
};


#endif //AUTOSCALING_PAXOS_THRESHOLD_COMPONENT_HPP
