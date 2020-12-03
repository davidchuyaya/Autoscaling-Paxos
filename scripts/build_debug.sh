#!/bin/bash

sudo cmake -DCMAKE_CXX_COMPILER=/usr/bin/g++ -DCMAKE_BUILD_TYPE=Debug .
sudo cmake --build . --target all -- -j 4
sudo mkdir build
find . -maxdepth 1 -type f -executable | xargs -I {} sudo mv {} build