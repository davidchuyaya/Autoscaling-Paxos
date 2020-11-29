#!/bin/bash

scripts/install_dependencies_for_running.sh
sudo apt-get install -y build-essential
sudo snap install cmake --channel=3.18/stable --classic