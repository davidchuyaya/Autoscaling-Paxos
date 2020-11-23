#!/bin/bash

wget https://github.com/aws/aws-sdk-cpp/archive/1.8.95.zip
unzip 1.8.95
cd aws-sdk-cpp-1.8.95
sudo cmake . -D CMAKE_BUILD_TYPE=Release -D BUILD_ONLY="ec2" -D ENABLE_TESTING=off -D AUTORUN_UNIT_TESTS=off
sudo make -j4