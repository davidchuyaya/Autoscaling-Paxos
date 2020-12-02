//
// Created by David Chu on 11/26/20.
//

#ifndef AUTOSCALING_PAXOS_ANNA_WRITE_ONLY_HPP
#define AUTOSCALING_PAXOS_ANNA_WRITE_ONLY_HPP

#include <shared_mutex>
#include <unordered_set>
#include <string>
#include "client/kvs_client.hpp"
#include "utils/config.hpp"

class anna_write_only {
public:
	explicit anna_write_only();
	void putSingletonSet(const std::string& key, const std::string& value);

private:
	std::shared_mutex clientMutex;
	KvsClient client;

	std::shared_mutex keysWrittenMutex;
	std::unordered_set<std::string> keysWritten = {};

	void listenerThread();
	void putLattice(const std::string& prefixedKey, const std::unordered_set<std::string>& lattice);
};


#endif //AUTOSCALING_PAXOS_ANNA_WRITE_ONLY_HPP
