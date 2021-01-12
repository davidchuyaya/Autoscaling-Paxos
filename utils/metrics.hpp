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
		NumReceivedMockMessages
	};
	const static std::unordered_map<Counter, std::string> counterNames = {
			{NumProcessedMessages, "num_processed_messages"},
			{NumSentMockMessages, "num_sent_mock_messages"},
			{NumReceivedMockMessages, "num_received_mock_messages"}
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
											  const std::unordered_set<Summary>& summaries);
	void addCounters(std::shared_ptr<variables> vars, const std::unordered_set<Counter>& counters);
	void addGauges(std::shared_ptr<variables> vars, const std::unordered_set<Gauge>& gauges);
	void addHistograms(std::shared_ptr<variables> vars, const std::unordered_set<Histogram>& histograms);
	void addSummaries(std::shared_ptr<variables> vars, const std::unordered_set<Summary>& summaries);
};


#endif //AUTOSCALING_PAXOS_METRICS_HPP
