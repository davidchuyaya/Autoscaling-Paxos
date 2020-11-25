#!/bin/bash

sudo wget https://github.com/protocolbuffers/protobuf/releases/download/v3.13.0/protobuf-all-3.13.0.zip
sudo unzip protobuf-all-3.13.0.zip
sudo rm -rf protobuf-all-3.13.0.zip
cd protobuf-3.13.0
sudo ./configure CXX=g++ CXXFLAGS='-std=c++17 -O3 -g'
# This steps will take a LONG time. Adjust based on your number of cores
sudo make -j4
cd ..