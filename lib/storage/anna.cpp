//
// Created by Taj Shaik on 11/5/20.
//

#include "anna.hpp"

#include <utility>

anna::anna(network* zmqNetwork, const std::unordered_map<std::string, std::string>& keyValues,
		   const annaListener& listener, bool writeOnly) : zmqNetwork(zmqNetwork), listener(listener) {
	LOG("Anna key prefix: {}", config::ANNA_KEY_PREFIX);

	startKeyAddressRequestListener();
	zmqNetwork->startAnnaReader(config::ANNA_KEY_ADDRESS_PORT, AnnaKeyAddress);
	if (!writeOnly) {
		startRequestListener();
		zmqNetwork->startAnnaReader(config::ANNA_RESPONSE_PORT, AnnaResponse);

		//resend subscriptions for keys we heard back from
		zmqNetwork->addTimer([&](const time_t now) {
			for (auto& [key, responded] : respondedToSubscribedKey) {
				if (responded) {
					LOG("Resending anna GET request for key {}", key);
					bool sent = tryRequest(message::createAnnaGetRequest(key));
					respondedToSubscribedKey[key] = !sent; //if message was just sent, then Anna has not responded to it yet
				}
			}
		}, 0, config::ANNA_RECHECK_SEC, true); //check immediately at time 0
	}
	writeKeyAddressSocket = zmqNetwork->startAnnaWriter(
			"tcp://" + config::ANNA_ROUTING_ADDRESS + ":" + std::to_string(kKeyAddressPort));

	//write all keys
	for (const auto&[key, value] : keyValues)
		putSingletonSet(key, value);
}

void anna::startKeyAddressRequestListener() {
	zmqNetwork->addHandler(AnnaKeyAddress, [&](const std::string& ipAddress, const std::string& payload,
	                                           const time_t time) {
		KeyAddressResponse response;
		response.ParseFromString(payload);
		LOG("Received anna key address: {}", response.ShortDebugString());
		const std::string& key = response.addresses(0).key();

		if (response.error() == AnnaError::NO_SERVERS) {
			BENCHMARK_LOG("No server for Anna key, retrying: {}", key);
			tryKeyAddressRequest(message::createAnnaKeyAddressRequest(key));
		}
		else if (addressForKey.find(key) == addressForKey.end()) {
			//only store the 1st routing address, since we don't have too much to write
			const std::string& address = response.addresses(0).ips(0);
			addressForKey[key] = address;
			pendingKeyAddresses.erase(key);
			//create the socket if this is the first time we're seeing this address
			if (socketForAddress.find(address) == socketForAddress.end())
				socketForAddress[address] = zmqNetwork->startAnnaWriter(address);
			//send pending writes
			if (pendingWrites.find(key) != pendingWrites.end()) {
				tryRequest(pendingWrites[key]);
				pendingWrites.erase(key);
			}
		}
		else
			LOG("Anna key address is a duplicate");
	});
}

void anna::startRequestListener() {
	zmqNetwork->addHandler(AnnaResponse, [&](const std::string& ipAddress, const std::string& payload,
	                                         const time_t now) {
		KeyResponse response;
		response.ParseFromString(payload);
		LOG("Received anna response: {}", response.ShortDebugString());

		const std::string& prefixedKey = response.tuples(0).key();
		switch (response.type()) {
			case PUT: {
				if (response.error() == NO_ERROR) //put succeeded
					return;
				BENCHMARK_LOG("ERROR: Anna PUT failed!?");
				break;
			}
			case GET: {
				respondedToSubscribedKey[prefixedKey] = true;
				if (response.error() != NO_ERROR) //wait for subscription loop to resend GET request
					return;
				if (lastPayloadForKey[prefixedKey] == payload) //cache subscription results, only process on change
					return;
				lastPayloadForKey[prefixedKey] = payload;
				for (const KeyTuple& keyTuple : response.tuples()) {
					SetValue setValue;
					setValue.ParseFromString(keyTuple.payload());
					std::unordered_set<std::string> payloadSet (setValue.values().begin(), setValue.values().end());

					two_p_set twoPset;
					std::string key = twoPset.mergeAndUnprefixKey(prefixedKey, payloadSet);
					if (twoPset.empty())
						return;
					listener(key, twoPset, now);
				}
				break;
			}
			default: {}
		}
	});
}

bool anna::tryRequest(const KeyRequest& request) {
	const std::string& key = request.tuples(0).key();

	//if we haven't fetched the address for this key yet, fetch & queue
	if (addressForKey.find(key) == addressForKey.end()) {
		if (pendingKeyAddresses.find(key) != pendingKeyAddresses.end()) //already queued
			return false;
		LOG("Anna queueing request to {}", key);
		if (request.type() == PUT) //GETs are not queued; they're subscriptions, so requests are sent periodically
			pendingWrites[key] = request;
		pendingKeyAddresses.emplace(key);
		tryKeyAddressRequest(message::createAnnaKeyAddressRequest(key));
		return false;
	}

	zmqNetwork->sendToServer(socketForAddress[addressForKey[key]]->socket, request.SerializeAsString());
	return true;
}

void anna::tryKeyAddressRequest(const KeyAddressRequest& request) {
	zmqNetwork->sendToServer(writeKeyAddressSocket->socket, request.SerializeAsString());
}

void anna::putSingletonSet(const std::string& key, const std::string& value) {
	putLattice(config::KEY_OBSERVED_PREFIX + key, {value});
}

void anna::removeSingletonSet(const std::string& key, const std::string& value) {
	putLattice(config::KEY_REMOVED_PREFIX + key, {value});
}

void anna::putLattice(const std::string& prefixedKey, const std::unordered_set<std::string>& lattice) {
	tryRequest(message::createAnnaPutRequest(prefixedKey, message::createAnnaSet(lattice).SerializeAsString()));
}

void anna::subscribeTo(const std::string& key) {
	respondedToSubscribedKey[config::KEY_OBSERVED_PREFIX + key] = true;
//	respondedToSubscribedKey[config::KEY_REMOVED_PREFIX + key] = true; TODO reenable subscription to removed sets?
}

void anna::unsubscribeFrom(const std::string& key) {
	respondedToSubscribedKey.erase(config::KEY_OBSERVED_PREFIX + key);
//	respondedToSubscribedKey.erase(config::KEY_REMOVED_PREFIX + key);
}