//
// Created by Taj Shaik on 11/3/20.
//

#ifndef AUTOSCALING_PAXOS_PARSER_HPP
#define AUTOSCALING_PAXOS_PARSER_HPP

#include <map>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace parser {
    std::map<int, std::string> parse_proposer(std::string file_name);
    std::map<int, std::map<int, std::string>> parse_acceptors(std::string file_name);
}

#endif //AUTOSCALING_PAXOS_PARSER_HPP
