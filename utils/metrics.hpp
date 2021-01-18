//
// Created by David Chu on 1/4/21.
//

#ifndef AUTOSCALING_PAXOS_METRICS_HPP
#define AUTOSCALING_PAXOS_METRICS_HPP

#include <prometheus/exposer.h>
#include <prometheus/counter.h>
#include <prometheus/registry.h>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include "config.hpp"

namespace metrics {
	enum Counter {
		NumProcessedMessages,
		NumSentMockMessages,
		NumReceivedMockMessages,
		NumIncomingMessages,
		NumOutgoingMessages,
		P1A,
		P1BSuccess,
		P1BPreempted,
		P2BPreempted,
		LeaderHeartbeatReceived
	};
	const static std::unordered_map<Counter, std::string> counterNames = {
			{NumProcessedMessages, "num_processed_messages"},
			{NumSentMockMessages, "num_sent_mock_messages"},
			{NumReceivedMockMessages, "num_received_mock_messages"},
			{NumIncomingMessages, "num_incoming_messages"},
			{NumOutgoingMessages, "num_outgoing_messages"},
			{P1A, "p1a"},
			{P1BSuccess, "p1b_success"},
			{P1BPreempted, "p1b_preempted"},
			{P2BPreempted, "p2b_preempted"},
			{LeaderHeartbeatReceived, "leader_heartbeat_received"}
	};
	enum Gauge {

	};
	enum Histogram {

	};
	enum Summary {

	};
	struct variables {
		std::shared_ptr<prometheus::Exposer> exposer; //to be scraped from
		std::shared_ptr<prometheus::Registry> registry;

		std::unordered_map<Counter, prometheus::Counter*> counters;
		std::unordered_map<Gauge, prometheus::Gauge*> gauges;
		std::unordered_map<Histogram, prometheus::Histogram*> histograms;
		std::unordered_map<Summary, prometheus::Summary*> summaries;
	};

	std::shared_ptr<variables> createMetricsVars(const std::unordered_set<Counter>& counters,
											  const std::unordered_set<Gauge>& gauges,
											  const std::unordered_set<Histogram>& histograms,
											  const std::unordered_set<Summary>& summaries,
											  const std::string& label);
	void addCounters(std::shared_ptr<variables> vars, const std::unordered_set<Counter>& counters,
	                 const std::string& label);
	void addGauges(std::shared_ptr<variables> vars, const std::unordered_set<Gauge>& gauges,
	               const std::string& label);
	void addHistograms(std::shared_ptr<variables> vars, const std::unordered_set<Histogram>& histograms,
	                   const std::string& label);
	void addSummaries(std::shared_ptr<variables> vars, const std::unordered_set<Summary>& summaries,
	                  const std::string& label);
};


#endif //AUTOSCALING_PAXOS_METRICS_HPP
