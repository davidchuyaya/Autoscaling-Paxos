//
// Created by David Chu on 10/4/20.
//

#include "network.hpp"

network::network(): context(config::ZMQ_NUM_IO_THREADS) {}

void network::poll() {
	time_t now;
	zmq::message_t message;

	while (true) {
		zmq::poll(pollItems.data(), pollItems.size(), config::ZMQ_POLL_TIMEOUT_SEC);
		time(&now); //Note: Since time is only calculated at the beginning, processing too long = slow clock, chaos ensues

		//Note: If we use a for-each loop instead, then we cannot add sockets along the way
		for (int i = 0; i < pollItems.size(); ++i) {
			if (pollItems[i].revents & ZMQ_POLLIN) {
				auto receiver = sockets[i];

				std::string senderAddress;
				if (receiver->isServer) {
					receiver->socket.recv(&message); //servers will receive the sender's address first
					senderAddress = zmqMessageToString(message);
				}
				else
					senderAddress = receiver->senderAddress;

				//TODO might be more performant to set DONTWAIT = true, then recv until we hit an error
				receiver->socket.recv(&message);
				handlers[receiver->type](senderAddress, zmqMessageToString(message), now);
			}
		}

		//Close sockets. Note: Closing a socket must be done at the end of polling, otherwise the iterator is screwed up
		removeClosedSockets();
		checkTimers(now);
	}
}

std::shared_ptr<socketInfo> network::startServerAtPort(int port, const ComponentType clientType) {
	auto server = sockets.emplace_back(std::make_shared<socketInfo>(
			socketInfo::serverSocket(context, clientType)));
	server->socket.setsockopt(ZMQ_LINGER, 0); //don't queue messages to closed sockets
	server->socket.bind("tcp://*:" + std::to_string(port));
	pollItems.emplace_back(zmq::pollitem_t{static_cast<void*>(server->socket), 0, ZMQ_POLLIN, 0});
	return server;
}

std::shared_ptr<socketInfo> network::connectToAddress(const std::string& address, const int port, const ComponentType serverType) {
	auto client = sockets.emplace_back(std::make_shared<socketInfo>(
			socketInfo::clientSocket(context, serverType, address)));
	client->socket.setsockopt(ZMQ_IDENTITY, config::IP_ADDRESS.c_str(), config::IP_ADDRESS.size()); //send router our IP
	client->socket.setsockopt(ZMQ_LINGER, 0);
	client->socket.connect("tcp://" + address + ":" + std::to_string(port));
	pollItems.emplace_back(zmq::pollitem_t{static_cast<void*>(client->socket), 0, ZMQ_POLLIN, 0});
	return client;
}

void network::connectExistingSocketToAddress(const std::shared_ptr<socketInfo>& client, const std::string& address) {
	client->socket.connect("tcp://" + address);
}

void network::sendToServer(zmq::socket_t& socket, const std::string& payload) {
	socket.send(stringToZmqMessage(payload));
}

void network::sendToClient(zmq::socket_t& socket, const std::string& clientAddress, const std::string& payload) {
	socket.send(stringToZmqMessage(clientAddress), ZMQ_SNDMORE);
	socket.send(stringToZmqMessage(payload));
}

void network::closeSocket(const std::shared_ptr<socketInfo>& socket) {
	//Note: Closing a socket must be done at the end of polling, otherwise the iterator is screwed up
	socketsToRemove.emplace(socket);
}

void network::addHandler(const ComponentType sender, const network::messageHandler& handler) {
	handlers[sender] = handler;
}

void network::addTimer(const network::timer& func, const int secondsInterval, const bool repeating) {
	time_t now;
	time(&now);
	addTimer(func, now, secondsInterval, repeating);
}

void network::addTimer(const network::timer& func, const time_t now, const int secondsInterval, const bool repeating) {
	time_t expiry = now + secondsInterval;
	timers.emplace(timerInfo{func, expiry, secondsInterval, repeating});
}

std::string network::zmqMessageToString(const zmq::message_t& message) {
	return std::string(static_cast<const char*>(message.data()), message.size());
}

zmq::message_t network::stringToZmqMessage(const std::string& string) {
	zmq::message_t message(string.size());
	memcpy(message.data(), string.c_str(), string.size());
	return message;
}

void network::removeClosedSockets() {
	if (socketsToRemove.empty())
		return;

	auto socketsIterator = sockets.begin();
	auto pollItemsIterator = pollItems.begin();
	auto socketsToRemoveIterator = socketsToRemove.begin();
	while (socketsToRemoveIterator != socketsToRemove.end()) {
		if (*socketsIterator == *socketsToRemoveIterator) {
			LOG("Socket closing at IP: {}", (*socketsIterator)->senderAddress);
			zmq_close((*socketsIterator)->socket);
			sockets.erase(socketsIterator);
			pollItems.erase(pollItemsIterator);
			socketsToRemove.erase(socketsToRemoveIterator);
		}
		else {
			socketsIterator++;
			pollItemsIterator++;
			socketsToRemoveIterator++;
		}
	}
}

void network::checkTimers(const time_t now) {
	if (timers.empty())
		return;
	while (true) {
		timerInfo next = timers.top();
		if (next.expiry > now) //found the first timer that didn't expire
			return;

		LOG("Timer triggered: expiry = {}, secondsInterval = {}", next.expiry, next.secondsInterval);
		next.function(now);
		timers.pop();

		//update expiry, push back into priority queue if it's a repeating timer
		if (!next.repeating)
			continue;
		LOG("Timer rescheduled: new expiry = {}", now + next.secondsInterval);
		timerInfo updatedNext {next.function, now + next.secondsInterval, next.secondsInterval, next.repeating};
		timers.push(updatedNext);
	}
}
