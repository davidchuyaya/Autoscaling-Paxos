#!/bin/bash

git clone https://github.com/jupp0r/prometheus-cpp.git
mkdir -p include
mv prometheus-cpp include/prometheus-cpp
cd include/prometheus-cpp
# fetch third-party dependencies
git submodule init
git submodule update

mkdir _build
cd _build

cmake .. -DBUILD_SHARED_LIBS=ON -DENABLE_TESTING=OFF
make -j 12
mkdir -p deploy
make DESTDIR=`pwd`/deploy install
cmake --install . --config Release