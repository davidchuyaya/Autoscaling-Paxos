//
// Created by David Chu on 11/12/20.
//

#ifndef AUTOSCALING_PAXOS_HEARTBEATER_HPP
#define AUTOSCALING_PAXOS_HEARTBEATER_HPP

#include <shared_mutex>
#include <thread>
#include <vector>
#include "../models/threshold_component.hpp"
#include "config.hpp"
#include "network.hpp"

namespace heartbeater {
    void heartbeat(std::shared_mutex& mutex, std::vector<int>& sockets) {
        std::thread thread([](std::shared_mutex& mutex, std::vector<int>& sockets){
	        const Heartbeat& message = message::createGenericHeartbeat();
            while (true) {
                std::this_thread::sleep_for(std::chrono::seconds(config::HEARTBEAT_SLEEP_SEC));
                std::shared_lock lock(mutex);
                for (const int socket : sockets) {
	                bool success = network::sendPayload(socket, message);
	                if (!success) //removed failed heartbeat destination
		                sockets.erase(std::remove(sockets.begin(), sockets.end(), socket), sockets.end());
                }
            }}, std::ref(mutex), std::ref(sockets));
        thread.detach();
    }
	template<typename SendMessage, typename ReceiveMessage>
    void heartbeat(const SendMessage& message, threshold_component<SendMessage, ReceiveMessage>& component) {
        std::thread thread([message](threshold_component<SendMessage, ReceiveMessage>& component){
        	mainThreadHeartbeat(message, component);
        	}, std::ref(component));
        thread.detach();
    }
	template<typename SendMessage, typename ReceiveMessage>
	[[noreturn]] void mainThreadHeartbeat(const SendMessage& message,
									   threshold_component<SendMessage, ReceiveMessage>& component) {
		while (true) {
			std::this_thread::sleep_for(std::chrono::seconds(config::HEARTBEAT_SLEEP_SEC));
			component.broadcast(message);
		}
	}
};


#endif //AUTOSCALING_PAXOS_HEARTBEATER_HPP
