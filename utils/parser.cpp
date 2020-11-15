//
// Created by Taj Shaik on 11/3/20.
//

#include "parser.hpp"

void parser::parseFileByLine(const std::string& fileName, const std::function<void(std::istringstream&)>& lineParser) {
    std::string line;
    std::ifstream file(fileName);
    while (std::getline(file, line))
    {
        std::istringstream iss(line);
        lineParser(iss);
    }
}

parser::idToIP parser::parseIDtoIPs(const std::string& fileName) {
    idToIP iDtoIPs;
    parseFileByLine(fileName, [&iDtoIPs](std::istringstream& iss) {
        int ID;
        std::string IP;
        iss >> ID >> IP;
        iDtoIPs[ID] = IP;
    });
    return iDtoIPs;
}

std::unordered_map<int, parser::idToIP> parser::parseAcceptors(const std::string& fileName) {
    std::unordered_map<int, parser::idToIP> acceptorGroupIDtoIDstoIPs;
    parseFileByLine(fileName, [&acceptorGroupIDtoIDstoIPs](std::istringstream& iss) {
        int groupID;
        int ID;
        std::string IP;
        iss >> groupID >> ID >> IP;
        acceptorGroupIDtoIDstoIPs[groupID][ID] = IP;
    });
    return acceptorGroupIDtoIDstoIPs;
}