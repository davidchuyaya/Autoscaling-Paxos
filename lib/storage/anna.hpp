//
// Created by Taj Shaik on 11/5/20.
//

#ifndef AUTOSCALING_PAXOS_ANNA_HPP
#define AUTOSCALING_PAXOS_ANNA_HPP

#include <unordered_map>
#include <unordered_set>
#include <string>
#include <functional>
#include <memory>
#include "threads.hpp"
#include "two_p_set.hpp"
#include "../../models/message.hpp"
#include "../../utils/config.hpp"
#include "../../utils/network.hpp"
#include "../../utils/uuid.hpp"

class anna {
public:
	using annaListener = std::function<void(const std::string& key, const two_p_set& twoPSet, const time_t now)>;

	static anna* writeOnly(network* zmqNetwork, const std::unordered_map<std::string, std::string>& keyValues) {
		return new anna(zmqNetwork, keyValues, [](const std::string& key, const two_p_set& twoPSet, const time_t now) {}, true);
	}
	static anna* readWritable(network* zmqNetwork, const std::unordered_map<std::string, std::string>& keyValues,
	                      const annaListener& listener) {
		return new anna(zmqNetwork, keyValues, listener, false);
	}
	void putSingletonSet(const std::string& key, const std::string& value);
	void removeSingletonSet(const std::string& key, const std::string& value);
    void subscribeTo(const std::string& key);
    void unsubscribeFrom(const std::string& key);
private:
	network* zmqNetwork;
	const annaListener listener;

	std::shared_ptr<socketInfo> writeKeyAddressSocket;
	std::unordered_map<std::string, std::string> addressForKey; //key, address
	std::unordered_map<std::string, std::shared_ptr<socketInfo>> socketForAddress; //address, socket
	std::unordered_set<std::string> pendingKeyAddresses; //key
	std::unordered_map<std::string, KeyRequest> pendingWrites; //key, value
	std::unordered_map<std::string, bool> respondedToSubscribedKey; //key, responded
	std::unordered_map<std::string, std::string> lastPayloadForKey; //key, payload string

	anna(network* zmqNetwork, const std::unordered_map<std::string, std::string>& keyValues,
	     const annaListener& listener, bool writeOnly);
	void startKeyAddressRequestListener();
	void startRequestListener();
    void putLattice(const std::string& prefixedKey, const std::unordered_set<std::string>& lattice);
    bool tryRequest(const KeyRequest& request);
	void tryKeyAddressRequest(const KeyAddressRequest& request);
};

#endif //AUTOSCALING_PAXOS_ANNA_HPP
