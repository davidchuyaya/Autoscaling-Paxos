//
// Created by David Chu on 1/4/21.
//

#include "metrics.hpp"

std::shared_ptr<metrics::variables> metrics::createMetricsVars(const std::unordered_set<Counter>& counters,
											  const std::unordered_set<Gauge>& gauges,
											  const std::unordered_set<Histogram>& histograms,
											  const std::unordered_set<Summary>& summaries) {
	std::shared_ptr<variables> vars = std::make_shared<variables>();
	vars->exposer = std::make_shared<prometheus::Exposer>(config::PRIVATE_IP_ADDRESS + ":" +
			std::to_string(config::PROMETHEUS_PORT), 1);
	vars->registry = std::make_shared<prometheus::Registry>();

	addCounters(vars, counters);
	addGauges(vars, gauges);
	addHistograms(vars, histograms);
	addSummaries(vars, summaries);

	vars->exposer->RegisterCollectable(vars->registry);
	return vars;
}

void metrics::addCounters(std::shared_ptr<variables> vars, const std::unordered_set<Counter>& counters) {
	for (const Counter& counter: counters) {
		prometheus::Family<prometheus::Counter>& family = prometheus::BuildCounter()
				.Name(counterNames.at(counter))
				.Register(*vars->registry);
		// add a counter to the metric family
		vars->counters[counter] = &family.Add({});
	}
}

void metrics::addGauges(std::shared_ptr<variables> vars, const std::unordered_set<Gauge>& gauges) {}
void metrics::addHistograms(std::shared_ptr<variables> vars, const std::unordered_set<Histogram>& histograms) {}
void metrics::addSummaries(std::shared_ptr<variables> vars, const std::unordered_set<Summary>& summaries) {}