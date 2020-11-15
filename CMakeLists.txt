cmake_minimum_required(VERSION 3.17)
project(Autoscaling_Paxos)
set(CMAKE_CXX_STANDARD 17)

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -pthread")

INCLUDE(ExternalProject)

# Begin Anna things -----------------------------------------------------------
SET(VENDOR_DIR common/vendor)
ADD_SUBDIRECTORY(${VENDOR_DIR}/spdlog)
ADD_SUBDIRECTORY(${VENDOR_DIR}/yamlcpp)
ADD_SUBDIRECTORY(${VENDOR_DIR}/zeromq)
ADD_SUBDIRECTORY(${VENDOR_DIR}/zeromqcpp)

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR})
INCLUDE_DIRECTORIES(${SPDLOG_INCLUDE_DIRS})
INCLUDE_DIRECTORIES(${ZEROMQCPP_INCLUDE_DIRS})
INCLUDE_DIRECTORIES(${ZEROMQ_INCLUDE_DIRS})
INCLUDE_DIRECTORIES(${YAMLCPP_INCLUDE_DIRS})
INCLUDE_DIRECTORIES(common/include)

INCLUDE(FindProtobuf)
FIND_PACKAGE(Protobuf REQUIRED)
INCLUDE_DIRECTORIES(${PROTOBUF_INCLUDE_DIR})
PROTOBUF_GENERATE_CPP(PROTO_SRC PROTO_HEADER
        ./common/proto/anna.proto
        ./common/proto/shared.proto
        models/message.proto) # Not Anna thing

ADD_LIBRARY(proto ${PROTO_HEADER} ${PROTO_SRC})

FILE(GLOB_RECURSE ZMQ_UTIL_SRC common/include/zmq/*.cpp)
FILE(GLOB_RECURSE ZMQ_UTIL_HEADER common/include/zmq/*.hpp)
ADD_LIBRARY(hydro-zmq STATIC ${ZMQ_UTIL_HEADER} ${ZMQ_UTIL_SRC})
ADD_DEPENDENCIES(hydro-zmq zeromq zeromqcpp spdlog)

LINK_DIRECTORIES(${ZEROMQ_LINK_DIRS} ${YAMLCPP_LINK_DIRS})

# End Anna things -------------------------------------------------------------


file(GLOB UTILS_SRC
        "utils/*.hpp"
        "utils/*.cpp"
        )

file(GLOB MODELS_SRC
        "models/*.hpp"
        "models/*.cpp"
        )
set(PROXY_LEADERS_SRC proxy_leader.cpp proxy_leader.hpp)
set(BATCHERS_SRC batcher.cpp batcher.hpp)
set(PROPOSER_SRC proposer.cpp proposer.hpp)
set(ACCEPTOR_SRC acceptor.cpp acceptor.hpp)
set(UNBATCHER_SRC unbatcher.cpp unbatcher.hpp)
set(MAIN_SRC main.cpp main.hpp)
set(STORAGE_SRC "lib/storage/storage.hpp")

include_directories(${CMAKE_CURRENT_BINARY_DIR})
add_executable(Autoscaling_Paxos ${MAIN_SRC} ${UTILS_SRC} ${STORAGE_SRC} ${MODELS_SRC} ${PROTO_SRC} ${PROTO_HDRS})
add_executable(acceptor ${ACCEPTOR_SRC} ${UTILS_SRC} ${STORAGE_SRC} ${MODELS_SRC} ${PROTO_SRC} ${PROTO_HDRS})
add_executable(batcher ${BATCHERS_SRC} ${UTILS_SRC} ${STORAGE_SRC} ${MODELS_SRC} ${PROTO_SRC} ${PROTO_HDRS})
add_executable(proposer ${PROPOSER_SRC} ${UTILS_SRC} ${STORAGE_SRC} ${MODELS_SRC} ${PROTO_SRC} ${PROTO_HDRS})
add_executable(proxy_leader ${PROXY_LEADERS_SRC} ${UTILS_SRC} ${STORAGE_SRC} ${MODELS_SRC} ${PROTO_SRC} ${PROTO_HDRS})
add_executable(unbatcher ${UNBATCHER_SRC} ${UTILS_SRC} ${STORAGE_SRC} ${MODELS_SRC} ${PROTO_SRC} ${PROTO_HDRS})

target_link_libraries(Autoscaling_Paxos proto ${PROTOBUF_LIBRARY})
target_link_libraries(acceptor proto ${PROTOBUF_LIBRARY})
target_link_libraries(batcher proto ${PROTOBUF_LIBRARY})
target_link_libraries(proposer proto ${PROTOBUF_LIBRARY})
target_link_libraries(proxy_leader proto ${PROTOBUF_LIBRARY})
target_link_libraries(unbatcher proto ${PROTOBUF_LIBRARY})
