//
// Created by David Chu on 11/26/20.
//

#include "anna_write_only.hpp"

anna_write_only::anna_write_only() : client({UserRoutingThread(config::ANNA_ROUTING_ADDRESS, 0)},
											config::IP_ADDRESS) {}

void anna_write_only::putSingletonSet(const std::string& key, const std::string& value) {
	putLattice(config::KEY_OBSERVED_PREFIX + key, {value});
}

void anna_write_only::putLattice(const std::string& prefixedKey, const std::unordered_set<std::string>& lattice) {
	std::unique_lock clientLock(clientMutex);
	client.put_async(prefixedKey, serialize(lattice), SET);
	clientLock.unlock();

	// determine if we need to spawn a new thread to check if the PUT was successful
	std::unique_lock keysWrittenLock(keysWrittenMutex);
	const bool shouldSpawnListener = keysWritten.empty();
	keysWritten.insert(prefixedKey);

	if (shouldSpawnListener) {
		std::thread listener([&]{ listenerThread();});
		listener.detach();
	}
}

void anna_write_only::listenerThread() {
	while (true) {
		std::unique_lock lock(clientMutex);
		const std::vector<KeyResponse>& responses = client.receive_async();
		lock.unlock();

		for (const KeyResponse& response : responses) {
			if (response.type() != PUT)
				continue;
			for (const KeyTuple& keyTuple : response.tuples()) {
				LOG("PUT response received for key: %s\n", keyTuple.key().c_str());
				//TODO catch TIMEOUT errors & maybe resend?
				std::unique_lock keysWrittenLock(keysWrittenMutex);
				keysWritten.erase(keyTuple.key());
				if (keysWritten.empty())
					return;
			}
		}
		std::this_thread::sleep_for(std::chrono::seconds(config::ZMQ_RECEIVE_RETRY_SEC));
	}
}