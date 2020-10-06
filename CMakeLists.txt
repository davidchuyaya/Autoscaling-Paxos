cmake_minimum_required(VERSION 3.17)
project(Autoscaling_Paxos)

set(CMAKE_CXX_STANDARD 17)

add_executable(Autoscaling_Paxos main.cpp main.hpp acceptor.cpp acceptor.hpp proposer.cpp proposer.hpp utils/networkNode.cpp utils/networkNode.hpp)