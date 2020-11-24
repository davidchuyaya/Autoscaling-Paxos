//
// Created by Taj Shaik on 11/5/20.
//

#include "anna.hpp"

//Required for any file that intends to use KvsClient, otherwise will get an error about kZmqUtil.
ZmqUtil zmq_util;
ZmqUtilInterface *kZmqUtil = &zmq_util;

anna::anna() : client({UserRoutingThread(config::ANNA_ROUTING_ADDRESS, 0)}, config::IP_ADDRESS) {}

anna::anna(const std::unordered_set<std::string>& initialKeysToListenTo,
           const std::function<void(const std::string&, const two_p_set&)>& listener) : anna() {
    keysToListenTo = initialKeysToListenTo;
    std::thread receive([&]{ listenerThread(listener);});
    receive.detach();
    std::thread get([&] { periodicGet2PSet(); });
    get.detach();
}

anna::anna(const std::string& key, const std::unordered_set<std::string>& keysToListenTo,
           const std::function<void(const std::string&, const two_p_set&)>& listener) : anna(keysToListenTo, listener) {
	putSingletonSet(key, config::IP_ADDRESS);
}

void anna::put2Pset(const std::string& key, const two_p_set& twoPSet) {
    if (!twoPSet.getObserved().empty())
        putLattice(config::KEY_OBSERVED_PREFIX + key, twoPSet.getObserved());
    if (!twoPSet.getRemoved().empty())
        putLattice(config::KEY_REMOVED_PREFIX + key, twoPSet.getRemoved());
}

void anna::putSingletonSet(const std::string& key, const std::string& value) {
	two_p_set set;
	set.add(value);
	put2Pset(key, set);
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
void anna::listenerThread(const std::function<void(const std::string&, const two_p_set&)>& listener) {
    while (true) {
	    std::unique_lock lock(clientMutex);
        const std::vector<KeyResponse>& responses = client.receive_async();
        lock.unlock();

        for (const KeyResponse& response : responses) {
            if (response.type() != GET)
                continue;
            for (const KeyTuple& keyTuple : response.tuples()) {
	            std::unique_lock requestedKeysLock(requestedKeysMutex);
	            requestedKeys.erase(keyTuple.key());
	            requestedKeysLock.unlock();

                if (keyTuple.lattice_type() != SET)
                    continue;

                two_p_set twoPset;
	            const std::string& key = twoPset.mergeAndUnprefixKey(keyTuple.key(),
																  deserialize_set(keyTuple.payload()));
	            LOG("Received set with key %s: %s\n", key.c_str(), twoPset.printSet().c_str());
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

void anna::putLattice(const std::string& prefixedKey, const SetLattice<std::string>& lattice) {
	std::unique_lock lock(clientMutex);
    client.put_async(prefixedKey, serialize(lattice), SET);
}