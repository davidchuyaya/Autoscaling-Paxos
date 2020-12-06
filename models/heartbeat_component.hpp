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

template<typename SendMessage, typename ReceiveMessage>
class heartbeat_component : public threshold_component<SendMessage, ReceiveMessage> {
public:
	explicit heartbeat_component(const int waitThreshold, const int port = 0,
	                    const WhoIsThis_Sender whoIsThis = WhoIsThis_Sender_null,
	                    const std::function<void(int, const ReceiveMessage&)>& listener = [](int, const ReceiveMessage&){}) :
	                    threshold_component<SendMessage, ReceiveMessage>(waitThreshold, port, whoIsThis, listener) {
		std::thread thread([&]{checkHeartbeats();});
		thread.detach();
	}
	void disconnectFromIp(const std::string& ipAddress) override {
		LOG("Removing dead member: {}\n", ipAddress);
		const int socket = this->ipToSocket[ipAddress];
		shutdown(socket, 1);
		this->components.erase(std::remove(this->components.begin(), this->components.end(), socket), this->components.end());
		slowComponents.erase(std::remove(slowComponents.begin(), slowComponents.end(), socket), slowComponents.end());
		heartbeats.erase(socket);
		this->ipToSocket.erase(ipAddress);
	}
	void addConnection(int socket) override {
		{
			std::scoped_lock lock(this->componentMutex, heartbeatMutex);
			this->components.emplace_back(socket);
			//add 1st heartbeat immediately after connection is made
			LOG("Heartbeat added for socket: {}\n", socket);
			time(&heartbeats[socket]);

			//check threshold
			if (!thresholdMet())
				return;
		}
		this->componentCV.notify_all();
	}
    void send(const SendMessage& payload) {
        std::shared_lock lock(this->componentMutex);
        if (!this->canSend) { //block if not enough connections
	        this->waitForThreshold(lock);
        }
        int socket = nextComponentSocket();
        bool success = network::sendPayload(socket, payload);

//        while (!success) { //remove this socket, try a different one
//	        lock.unlock();
//	        {
//		        std::scoped_lock lock2(this->ipToSocketMutex, this->componentMutex);
//		        this->removeConnection(socket);
//	        }
//	        lock.lock();
//
//	        socket = nextComponentSocket();
//	        success = network::sendPayload(socket, payload);
//        }
    }
    void addHeartbeat(int socket) {
	    std::unique_lock lock(heartbeatMutex);
	    time(&heartbeats[socket]);
	}
private:
    std::shared_mutex heartbeatMutex;
    std::unordered_map<int, time_t> heartbeats = {}; //key = socket

    std::vector<int> slowComponents = {};
    int next = 0;

    [[nodiscard]] bool thresholdMet() const override {
	    return this->components.size() + slowComponents.size() >= this->waitThreshold;
    }
    int nextComponentSocket() {
	    //prioritize sending to fast proxy leaders
	    if (!this->components.empty()) {
		    next = (next + 1) % this->components.size();
		    return this->components[next];
	    }
	    else {
		    next = (next + 1) % slowComponents.size();
		    return slowComponents[next];
	    }
    }
    [[noreturn]] void checkHeartbeats() {
	    time_t now;
	    while (true) {
		    std::this_thread::sleep_for(std::chrono::seconds(config::HEARTBEAT_TIMEOUT_SEC));

		    std::shared_lock heartbeatLock(heartbeatMutex, std::defer_lock);
		    std::scoped_lock lock(heartbeatLock, this->componentMutex);
		    time(&now);

		    //if a node has no recent heartbeat, move it into the slow list
		    auto iterator = this->components.begin();
		    while (iterator != this->components.end()) {
			    const int socket = *iterator;
			    if (difftime(now, heartbeats[socket]) > config::HEARTBEAT_TIMEOUT_SEC) {
				    LOG("Node failed to heartbeat\n");
				    slowComponents.emplace_back(socket);
				    iterator = this->components.erase(iterator);
			    }
			    else
				    ++iterator;
		    }

		    //if a node has a heartbeat, move it into the fast list
		    iterator = slowComponents.begin();
		    while (iterator != slowComponents.end()) {
			    const int socket = *iterator;
			    if (difftime(now, heartbeats[socket]) < config::HEARTBEAT_TIMEOUT_SEC) {
				    this->components.emplace_back(socket);
				    iterator = slowComponents.erase(iterator);
			    }
			    else
				    ++iterator;
		    }
	    }
    }
};


#endif //AUTOSCALING_PAXOS_HEARTBEAT_COMPONENT_HPP
