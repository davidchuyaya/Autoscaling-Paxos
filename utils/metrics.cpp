//
// Created by David Chu on 1/4/21.
//

#include "metrics.hpp"

std::shared_ptr<metrics::variables> metrics::createMetricsVars(const std::unordered_set<Counter>& counters,
											  const std::unordered_set<Gauge>& gauges,
											  const std::unordered_set<Histogram>& histograms,
											  const std::unordered_set<Summary>& summaries,
											  const std::string& label) {
	std::shared_ptr<variables> vars = std::make_shared<variables>();
	vars->exposer = std::make_shared<prometheus::Exposer>(config::PRIVATE_IP_ADDRESS + ":" +
			std::to_string(config::PROMETHEUS_PORT), 1);
	vars->registry = std::make_shared<prometheus::Registry>();

	addCounters(vars, counters, label);
	addGauges(vars, gauges, label);
	addHistograms(vars, histograms, label);
	addSummaries(vars, summaries, label);

	vars->exposer->RegisterCollectable(vars->registry);
	return vars;
}

void metrics::addCounters(std::shared_ptr<variables> vars, const std::unordered_set<Counter>& counters,
                          const std::string& label) {
	for (const Counter& counter: counters) {
		prometheus::Family<prometheus::Counter>& family = prometheus::BuildCounter()
				.Labels({{"Type", label}})
				.Name(counterNames.at(counter))
				.Register(*vars->registry);
		// add a counter to the metric family
		vars->counters[counter] = &family.Add({});
	}
}

void metrics::addGauges(std::shared_ptr<variables> vars, const std::unordered_set<Gauge>& gauges,
                        const std::string& label) {}
void metrics::addHistograms(std::shared_ptr<variables> vars, const std::unordered_set<Histogram>& histograms,
                            const std::string& label) {}
void metrics::addSummaries(std::shared_ptr<variables> vars, const std::unordered_set<Summary>& summaries,
                           const std::string& label) {}