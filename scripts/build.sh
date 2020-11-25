#!/bin/bash

sudo cmake -DCMAKE_CXX_COMPILER=/usr/bin/g++ .
sudo cmake --build . --target all -- -j 4