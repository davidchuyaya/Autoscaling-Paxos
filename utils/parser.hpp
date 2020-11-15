//
// Created by Taj Shaik on 11/3/20.
//

#ifndef AUTOSCALING_PAXOS_PARSER_HPP
#define AUTOSCALING_PAXOS_PARSER_HPP

#include <functional>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace parser {
    using idToIP = std::unordered_map<int, std::string>;

    void parseFileByLine(const std::string& fileName, const std::function<void(std::istringstream&)>& lineParser);
    idToIP parseIDtoIPs(const std::string& fileName);
    std::unordered_map<int, idToIP> parseAcceptors(const std::string& fileName);
}

#endif //AUTOSCALING_PAXOS_PARSER_HPP
