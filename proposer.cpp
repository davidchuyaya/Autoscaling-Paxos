//
// Created by David Chu on 10/4/20.
//
#include <iostream>
#include "proposer.hpp"


proposer::proposer() {
    std::cout << "proposer is live!" << std::endl;
    startListening();
}

void proposer::startListening() {
    int serverSocket = -1;
    while (serverSocket == -1) {
        serverSocket = network::connectToServerAtAddress("127.0.0.1", 10000);
    }
    printf("Connected to server at socket: %d\n", serverSocket);
//    while (true) {
//
//    }
}
