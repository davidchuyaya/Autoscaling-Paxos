//
// Created by Taj Shaik on 11/5/20.
//

#include "anna.hpp"

#include <utility>

//Required for any file that intends to use KvsClient, otherwise will get an error about kZmqUtil.
ZmqUtil zmq_util;
ZmqUtilInterface *kZmqUtil = &zmq_util;

anna* anna::writeOnly(const std::unordered_map<std::string, std::string>& keyValues) {
	return new anna(true, keyValues, [](const std::string& key, const two_p_set& twoPSet){});
}

anna* anna::readWritable(const std::unordered_map<std::string, std::string>& keyValues, const annaListener& listener) {
	return new anna(false, keyValues, listener);
}

anna::anna(const bool isWriteOnly, const std::unordered_map<std::string, std::string>& keyValues,
		   const annaListener& listener) : isWriteOnly(isWriteOnly),
		client({UserRoutingThread(config::ANNA_ROUTING_ADDRESS, 0)}, config::IP_ADDRESS) {
	LOG("Anna address: {}", config::ANNA_ROUTING_ADDRESS);
	LOG("IP: {}", config::IP_ADDRESS);
	LOG("Anna key prefix: {}", config::ANNA_KEY_PREFIX);
	for (const auto&[key, value] : keyValues) {
		putSingletonSet(key, value);
	}
	std::thread receive([&]{ listenerThread(listener);});
	receive.detach();
}

void anna::putSingletonSet(const std::string& key, const std::string& value) {
	putLattice(config::KEY_OBSERVED_PREFIX + key, {value});
}

void anna::removeSingletonSet(const std::string& key, const std::string& value) {
	putLattice(config::KEY_REMOVED_PREFIX + key, {value});
}

void anna::putLattice(const std::string& prefixedKey, const std::unordered_set<std::string>& lattice) {
	const std::string& value = serialize(lattice);

	if (isWriteOnly) { //only need to count down how many puts we're waiting on if this is write only
		std::scoped_lock lock(clientMutex, pendingPutsMutex);
		client.put_async(prefixedKey, value, SET);
		pendingPuts.emplace(prefixedKey);
	}
	else {
		std::unique_lock lock(clientMutex);
		client.put_async(prefixedKey, value, SET);
	}
}

void anna::subscribeTo(const std::string& key) {
	std::scoped_lock lock(clientMutex, keysToListenToMutex);
	client.get_async(config::KEY_OBSERVED_PREFIX + key);
	client.get_async(config::KEY_REMOVED_PREFIX + key);
	keysToListenTo.emplace(config::KEY_OBSERVED_PREFIX + key);
	keysToListenTo.emplace(config::KEY_REMOVED_PREFIX + key);
}

void anna::unsubscribeFrom(const std::string& key) {
    std::unique_lock lock(keysToListenToMutex);
    keysToListenTo.erase(config::KEY_OBSERVED_PREFIX + key);
	keysToListenTo.erase(config::KEY_REMOVED_PREFIX + key);
}

void anna::listenerThread(const annaListener& listener) {
    while (true) {
	    std::this_thread::sleep_for(std::chrono::seconds(config::ANNA_RECHECK_SEC));

	    std::unique_lock lock(clientMutex);
        const std::vector<KeyResponse>& responses = client.receive_async();
        lock.unlock();

        for (const KeyResponse& response : responses) {
	        switch (response.type()) {
		        case PUT: {
			        LOG("PUT response received: {}\n", response.ShortDebugString());
			        //put succeeded
			        if (response.error() == NO_ERROR) {
			        	//nothing to do unless we're write-only, then we can stop looping once all puts succeed
			        	if (!isWriteOnly)
					        continue;

			        	std::unique_lock pendingPutsLock(pendingPutsMutex);
				        for (const KeyTuple& keyTuple : response.tuples())
					        pendingPuts.erase(keyTuple.key());
				        if (pendingPuts.empty())
					        return;
			        }
			        //put failed, retry
			        else {
				        std::unique_lock lock2(clientMutex);
				        for (const KeyTuple& keyTuple : response.tuples())
					        client.put_async(keyTuple.key(), keyTuple.payload(), keyTuple.lattice_type());
				        lock2.unlock();
			        }
		        }
		        case GET: {
			        LOG("GET response received: {}\n", response.ShortDebugString());
			        if (response.error() == NO_ERROR) {
				        for (const KeyTuple& keyTuple : response.tuples()) {
					        two_p_set twoPset = {};
					        std::string key = twoPset.mergeAndUnprefixKey(keyTuple.key(),
														   deserialize_set(keyTuple.payload()));
					        LOG("Received set with key {}: {}\n", key, twoPset.printSet());
					        listener(key, twoPset);
				        }
			        }

			        //as long as we're still subscribed, request the value again
			        std::scoped_lock lock2(keysToListenToMutex, clientMutex);
			        for (const KeyTuple& keyTuple : response.tuples()) {
			        	if (keysToListenTo.find(keyTuple.key()) != keysToListenTo.end())
					        client.get_async(keyTuple.key());
			        }
		        }
		        default: {}
	        }
        }
    }
}