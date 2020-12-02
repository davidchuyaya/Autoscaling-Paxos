#!/bin/bash

sudo cmake -DCMAKE_CXX_COMPILER=/usr/bin/g++ -DCMAKE_BUILD_TYPE=Release .
sudo cmake --build . --target all -- -j 4