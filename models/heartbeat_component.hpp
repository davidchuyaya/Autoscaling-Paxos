//
// Created by David Chu on 11/11/20.
//

#ifndef AUTOSCALING_PAXOS_HEARTBEAT_COMPONENT_HPP
#define AUTOSCALING_PAXOS_HEARTBEAT_COMPONENT_HPP

#include <shared_mutex>
#include <condition_variable>
#include <unordered_map>
#include <vector>
#include <thread>
#include <functional>
#include <algorithm>

#include "../lib/storage/two_p_set.hpp"
#include "message.pb.h"
#include "../utils/network.hpp"
#include "message.hpp"
#include "threshold_component.hpp"

class heartbeat_component : public threshold_component {
public:
    explicit heartbeat_component(int waitThreshold);

	template<typename Message>
    void connectAndListen(const two_p_set& newMembers, int port, const WhoIsThis_Sender& whoIsThis,
                          const std::function<void(int, const Message&)>& listener) {
		std::unique_lock membersLock(membersMutex);
		const two_p_set& updates = members.updatesFrom(newMembers);
		if (updates.empty())
			return;
		members.merge(updates);
		membersLock.unlock();

		for (const std::string& ip : updates.getObserved()) {
			LOG("Connecting to new member: {}\n", ip);
			std::thread thread([&, ip, whoIsThis, port, listener]{
				const int socket = network::connectToServerAtAddress(ip, port, whoIsThis);
				std::unique_lock lock(ipToSocketMutex);
				ipToSocket[ip] = socket;
				lock.unlock();
				heartbeat_component::addConnection(socket); //TODO figure out why virtual func isn't working normally
				network::listenToSocketUntilClose(socket, listener);
			});
			thread.detach();
		}

		if (!updates.getRemoved().empty()) {
			std::scoped_lock lock(ipToSocketMutex, componentMutex, heartbeatMutex);
			for (const std::string& ip : updates.getRemoved()) {
				LOG("Removing dead member: {}\n", ip);
				const int socket = ipToSocket[ip];
				shutdown(socket, 1);
				components.erase(std::remove(components.begin(), components.end(), socket), components.end());
				slowComponents.erase(std::remove(slowComponents.begin(), slowComponents.end(), socket), slowComponents.end());
				heartbeats.erase(socket);
				ipToSocket.erase(ip);
			}
		}
	}
	void addConnection(int socket) override;
    template<typename Message> void send(const Message& payload) {
        std::shared_lock lock(componentMutex);
        if (!canSend) { //block if not enough connections
            waitForThreshold(lock);
        }
        int socket = nextComponentSocket();
        network::sendPayload(socket, payload);
    }
    void addHeartbeat(int socket);
private:
    std::shared_mutex heartbeatMutex;
    std::unordered_map<int, time_t> heartbeats = {}; //key = socket

    std::shared_mutex ipToSocketMutex;
    std::unordered_map<std::string, int> ipToSocket = {};

    std::vector<int> slowComponents = {};
    int next = 0;

    bool thresholdMet() override;
    int nextComponentSocket();
    [[noreturn]] void checkHeartbeats();

    using threshold_component::connectAndMaybeListen;
};


#endif //AUTOSCALING_PAXOS_HEARTBEAT_COMPONENT_HPP
