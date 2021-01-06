#!/bin/bash

scripts/install_dependencies_for_running.sh
sudo apt-get install -y build-essential zlib1g-dev
sudo snap install cmake --classic