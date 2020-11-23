#!/bin/bash

sudo cmake -DCMAKE_CXX_COMPILER=/usr/bin/g++ -DCMAKE_BUILD_TYPE=Debug -B build-paxos .
sudo cmake --build build-paxos --target all -- -j 4