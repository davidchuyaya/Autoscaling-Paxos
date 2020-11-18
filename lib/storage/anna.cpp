//
// Created by Taj Shaik on 11/5/20.
//

#include "anna.hpp"

//Required for any file that intends to use KvsClient, otherwise will get an error about kZmqUtil.
ZmqUtil zmq_util;
ZmqUtilInterface *kZmqUtil = &zmq_util;

anna::anna(const std::optional<std::function<void(two_p_set&)>>& listener) :
    client({UserRoutingThread(network::getIp(), 0)}, network::getIp()) {
    if (listener.has_value()) {
        std::thread t([&]{
            listenerThread(listener.value());
        });
        t.detach();
    }
}

anna::anna(const std::string& key, const std::string& value, const std::optional<std::function<void(two_p_set&)>>& listener) :
    anna(listener) {
    two_p_set valueSet;
    valueSet.add(value);
    put2Pset(config::KEY_OBSERVED_PREFIX + key, valueSet);
}

void anna::periodicGet2PSet(const std::string& key) {
    std::thread t([&]{
        while (true) {
            client.get_async(config::KEY_OBSERVED_PREFIX + key);
            client.get_async(config::KEY_REMOVED_PREFIX + key);
            std::this_thread::sleep_for(std::chrono::seconds(config::ANNA_RECHECK_SEC));
        }
    });
    t.detach();
}

void anna::put2Pset(const std::string& key, const two_p_set& twoPSet) {
    if (!twoPSet.getObserved().empty())
        putLattice(config::KEY_OBSERVED_PREFIX + key, twoPSet.getObserved());
    if (!twoPSet.getRemoved().empty())
        putLattice(config::KEY_REMOVED_PREFIX + key, twoPSet.getRemoved());
}

[[noreturn]]
void anna::listenerThread(const std::function<void(two_p_set&)>& listener) {
    while (true) {
        const std::vector<KeyResponse>& responses = client.receive_async();
        for (const KeyResponse& response : responses) {
            if (response.type() != GET)
                continue;
            for (const KeyTuple& keyTuple : response.tuples()) {
                if (keyTuple.lattice_type() != SET)
                    continue;

                two_p_set twoPset;
                twoPset.merge(keyTuple.key(), deserialize_set(keyTuple.payload()));
                listener(twoPset);
            }
        }
        std::this_thread::sleep_for(std::chrono::seconds(config::ZMQ_RECEIVE_RETRY_SEC));
    }
}

void anna::putLattice(const std::string& key, const SetLattice<std::string>& lattice) {
    client.put_async(key, serialize(lattice), SET);
}
