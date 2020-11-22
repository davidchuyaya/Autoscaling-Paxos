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
	put2Pset(config::KEY_OBSERVED_PREFIX + key, set);
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
        const std::vector<KeyResponse>& responses = client.receive_async();
        for (const KeyResponse& response : responses) {
            if (response.type() != GET)
                continue;
            for (const KeyTuple& keyTuple : response.tuples()) {
                if (keyTuple.lattice_type() != SET)
                    continue;

                two_p_set twoPset;
                const std::string& strippedKey = twoPset.mergeAndStripKey(keyTuple.key(), deserialize_set(keyTuple.payload()));
                listener(strippedKey, twoPset);
            }
        }
        std::this_thread::sleep_for(std::chrono::seconds(config::ZMQ_RECEIVE_RETRY_SEC));
    }
}

void anna::periodicGet2PSet() {
    while (true) {
        std::shared_lock lock(keysToListenToMutex);
        for (const std::string& key : keysToListenTo) {
            client.get_async(config::KEY_OBSERVED_PREFIX + key);
            client.get_async(config::KEY_REMOVED_PREFIX + key);
        }
        lock.unlock();
        std::this_thread::sleep_for(std::chrono::seconds(config::ANNA_RECHECK_SEC));
    }
}

void anna::putLattice(const std::string& key, const SetLattice<std::string>& lattice) {
    client.put_async(key, serialize(lattice), SET);
}
