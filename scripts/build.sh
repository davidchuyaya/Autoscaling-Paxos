#!/bin/bash

sudo cmake -DCMAKE_CXX_COMPILER=/usr/bin/g++ -DCMAKE_BUILD_TYPE=Release .
sudo cmake --build . --target all -- -j 4
sudo mkdir -p build
find . -maxdepth 1 -type f -executable | xargs -I {} sudo mv {} build