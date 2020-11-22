//
// Created by David Chu on 11/12/20.
//

#ifndef AUTOSCALING_PAXOS_HEARTBEATER_HPP
#define AUTOSCALING_PAXOS_HEARTBEATER_HPP

#include <shared_mutex>
#include <thread>
#include <vector>
#include <models/threshold_component.hpp>
#include "config.hpp"
#include "network.hpp"

namespace heartbeater {
    template<typename Message>
    void heartbeat(const Message& message, std::shared_mutex& mutex, std::vector<int>& sockets) {
        std::thread thread([&, message]{
            while (true) {
                std::this_thread::sleep_for(std::chrono::seconds(config::HEARTBEAT_SLEEP_SEC));
                std::shared_lock lock(mutex);
                for (const int socket : sockets)
                    network::sendPayload(socket, message);
            }});
        thread.detach();
    }
    template<typename Message>
    void heartbeat(const Message& message, threshold_component& component) {
        std::thread thread([&, message]{
            while (true) {
                std::this_thread::sleep_for(std::chrono::seconds(config::HEARTBEAT_SLEEP_SEC));
                component.broadcast(message);
        }});
        thread.detach();
    }
    template<typename Message>
    void heartbeat(const Message& message, std::shared_mutex& mutex, std::unordered_map<int, int>& sockets);
    template<typename Message>
    void heartbeat(const Message& message, std::shared_mutex& mutex, std::unordered_map<int, int>& sockets) {
        std::thread thread([&, message]{
            while (true) {
                std::this_thread::sleep_for(std::chrono::seconds(config::HEARTBEAT_SLEEP_SEC));
                std::shared_lock lock(mutex);
                for (const auto& [something, socket] : sockets)
                    network::sendPayload(socket, message);
        }});
        thread.detach();
    }
};


#endif //AUTOSCALING_PAXOS_HEARTBEATER_HPP
