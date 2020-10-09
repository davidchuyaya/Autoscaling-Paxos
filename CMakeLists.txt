cmake_minimum_required(VERSION 3.17)
project(Autoscaling_Paxos)
set(CMAKE_CXX_STANDARD 17)

include(FindProtobuf)
find_package(Protobuf REQUIRED)
include_directories(${PROTOBUF_INCLUDE_DIR})
PROTOBUF_GENERATE_CPP(PROTO_SRCS PROTO_HDRS models/message.proto)
add_library(proto ${PROTO_SRCS} ${PROTO_HDRS})

include_directories(${CMAKE_CURRENT_BINARY_DIR})
add_executable(Autoscaling_Paxos main.cpp main.hpp acceptor.cpp acceptor.hpp proposer.cpp proposer.hpp utils/networkNode.cpp utils/networkNode.hpp utils/config.hpp models/log.cpp models/log.hpp models/message.hpp models/message.cpp ${PROTO_SRC} ${PROTO_HDRS})
target_link_libraries(Autoscaling_Paxos proto ${PROTOBUF_LIBRARY})