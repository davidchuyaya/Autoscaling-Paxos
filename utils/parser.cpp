//
// Created by Taj Shaik on 11/3/20.
//

#include "parser.hpp"

std::map<int, std::string> parser::parse_proposer(std::string file_name) {
    std::map<int, std::string> proposer_map;
    std::string line;
    std::ifstream proposer_file(file_name);
    while (std::getline(proposer_file, line))
    {
        std::istringstream iss(line);
        int proposer_id;
        std::string proposer_ip_addr;
        if (!(iss >> proposer_id >> proposer_ip_addr))
            break;
        proposer_map[proposer_id] = proposer_ip_addr;
    }
    return proposer_map;
}

std::map<int, std::map<int, std::string>> parser::parse_acceptors(std::string file_name) {
    std::map<int, std::map<int, std::string>> acceptor_map;
    std::string line;
    std::ifstream proposer_file(file_name);
    while (std::getline(proposer_file, line))
    {
        std::istringstream iss(line);
        int acceptor_group, acceptor_id;
        std::string acceptor_ip_addr;
        if (!(iss >> acceptor_group >> acceptor_id >> acceptor_ip_addr))
            break;
        acceptor_map[acceptor_group][acceptor_id] = acceptor_ip_addr;
    }
    return acceptor_map;
}