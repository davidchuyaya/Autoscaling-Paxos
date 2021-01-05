#!/bin/bash

mkdir -p include/prometheus-cpp/_build
cd include/prometheus-cpp/_build
cmake .. -DBUILD_SHARED_LIBS=ON -DENABLE_TESTING=OFF
make -j 4
mkdir -p deploy
make DESTDIR=`pwd`/deploy install