//
// Created by Taj Shaik on 11/5/20.
//

#include "anna.hpp"

//Required for any file that intends to use KvsClient, otherwise will get an error about kZmqUtil.
ZmqUtil zmq_util;
ZmqUtilInterface *kZmqUtil = &zmq_util;

anna::anna(const std::unordered_set<std::string>& initialKeysToListenTo,
           const annaListener& listener) :
           client({UserRoutingThread(config::ANNA_ROUTING_ADDRESS, 0)}, config::IP_ADDRESS) {
    keysToListenTo = initialKeysToListenTo;
    std::thread receive([&]{ listenerThread(listener);});
    receive.detach();
    std::thread get([&] { periodicGet2PSet(); });
    get.detach();
}

anna::anna(const std::string& key, const std::unordered_set<std::string>& keysToListenTo,
           const annaListener& listener) : anna(keysToListenTo, listener) {
	putSingletonSet(key, config::IP_ADDRESS);
}

void anna::putSingletonSet(const std::string& key, const std::string& value) {
	putLattice(config::KEY_OBSERVED_PREFIX + key, {value});
}

void anna::putLattice(const std::string& prefixedKey, const std::unordered_set<std::string>& lattice) {
	std::unique_lock lock(clientMutex);
	client.put_async(prefixedKey, serialize(lattice), SET);
}

void anna::subscribeTo(const std::string& key) {
    std::unique_lock lock(keysToListenToMutex);
    keysToListenTo.emplace(key);
}

void anna::unsubscribeFrom(const std::string& key) {
    std::unique_lock lock(keysToListenToMutex);
    keysToListenTo.erase(key);
}

[[noreturn]]
void anna::listenerThread(const annaListener& listener) {
    while (true) {
	    std::unique_lock lock(clientMutex);
        const std::vector<KeyResponse>& responses = client.receive_async();
        lock.unlock();

        for (const KeyResponse& response : responses) {
            if (response.type() != GET) {
	            LOG("PUT response received: {}\n", response.ShortDebugString());
	            continue;
            }
            for (const KeyTuple& keyTuple : response.tuples()) {
	            std::unique_lock requestedKeysLock(requestedKeysMutex);
	            requestedKeys.erase(keyTuple.key());
	            requestedKeysLock.unlock();

                if (keyTuple.lattice_type() != SET)
                    continue;

                two_p_set twoPset = {};
	            std::string key = twoPset.mergeAndUnprefixKey(keyTuple.key(), deserialize_set(keyTuple.payload()));
	            LOG("Received set with key {}: {}\n", key, twoPset.printSet());
	            listener(key, twoPset);
            }
        }
        std::this_thread::sleep_for(std::chrono::seconds(config::ZMQ_RECEIVE_RETRY_SEC));
    }
}

void anna::periodicGet2PSet() {
    while (true) {
	    std::this_thread::sleep_for(std::chrono::seconds(config::ANNA_RECHECK_SEC));

	    std::shared_lock keysLock(keysToListenToMutex, std::defer_lock);
	    std::scoped_lock lock(keysLock, clientMutex, requestedKeysMutex);
        for (const std::string& key : keysToListenTo) {
        	//only request keys if we've heard a response for it
	        const std::string& observedKey = config::KEY_OBSERVED_PREFIX + key;
	        if (requestedKeys.find(observedKey) == requestedKeys.end()) {
		        client.get_async(observedKey);
		        requestedKeys.emplace(observedKey);
	        }
	        const std::string& removedKey = config::KEY_REMOVED_PREFIX + key;
	        if (requestedKeys.find(removedKey) == requestedKeys.end()) {
		        client.get_async(removedKey);
		        requestedKeys.emplace(removedKey);
	        }
        }
    }
}