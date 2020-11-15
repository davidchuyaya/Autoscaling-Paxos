cmake_minimum_required(VERSION 3.17)
project(Autoscaling_Paxos)
set(CMAKE_CXX_STANDARD 17)

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -pthread")

INCLUDE(ExternalProject)

EXTERNALPROJECT_ADD(zeromq
        URL "https://github.com/zeromq/libzmq/releases/download/v4.2.5/zeromq-4.2.5.tar.gz"
        PREFIX ${CMAKE_CURRENT_BINARY_DIR}
        BUILD_IN_SOURCE 1
        UPDATE_COMMAND ""
        CONFIGURE_COMMAND ./configure
        BUILD_COMMAND make
        INSTALL_COMMAND ""
        )

EXTERNALPROJECT_ADD(zeromqcpp
        GIT_REPOSITORY "https://github.com/zeromq/cppzmq.git"
        GIT_TAG "v4.3.0"
        BUILD_IN_SOURCE 1
        UPDATE_COMMAND ""
        CONFIGURE_COMMAND ""
        BUILD_COMMAND ""
        INSTALL_COMMAND ""
        )

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

file(GLOB ANNA_SRC
        "lib/storage/anna-client/*/*.hpp"
        "lib/storage/anna-client/*/*.hpp"
        "lib/storage/anna-client/*.hpp"
        "lib/storage/anna-client/*.cpp"
        )

include(FindProtobuf)
find_package(Protobuf REQUIRED)
include_directories(${PROTOBUF_INCLUDE_DIR})
PROTOBUF_GENERATE_CPP(PROTO_SRCS PROTO_HDRS models/message.proto)
add_library(proto ${PROTO_SRCS} ${PROTO_HDRS})

PROTOBUF_GENERATE_CPP(ANNA_PROTO_SRC ANNA_PROTO_HDRS lib/storage/anna-client/anna.proto lib/storage/anna-client/shared.proto)
add_library(anna ${ANNA_SRC} ${ANNA_PROTO_SRC} ${ANNA_PROTO_HDRS})
add_dependencies(anna zeromq)
add_dependencies(anna zeromqcpp)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
add_executable(Autoscaling_Paxos ${MAIN_SRC} ${UTILS_SRC} ${STORAGE_SRC} ${MODELS_SRC} ${PROTO_SRC} ${PROTO_HDRS})
add_executable(acceptor ${ACCEPTOR_SRC} ${UTILS_SRC} ${STORAGE_SRC} ${MODELS_SRC} ${PROTO_SRC} ${PROTO_HDRS})
add_executable(batcher ${BATCHERS_SRC} ${UTILS_SRC} ${STORAGE_SRC} ${MODELS_SRC} ${PROTO_SRC} ${PROTO_HDRS})
add_executable(proposer ${PROPOSER_SRC} ${UTILS_SRC} ${STORAGE_SRC} ${MODELS_SRC} ${PROTO_SRC} ${PROTO_HDRS})
add_executable(proxy_leader ${PROXY_LEADERS_SRC} ${UTILS_SRC} ${STORAGE_SRC} ${MODELS_SRC} ${PROTO_SRC} ${PROTO_HDRS})
add_executable(unbatcher ${UNBATCHER_SRC} ${UTILS_SRC} ${STORAGE_SRC} ${MODELS_SRC} ${PROTO_SRC} ${PROTO_HDRS})

target_link_libraries(Autoscaling_Paxos proto anna ${PROTOBUF_LIBRARY})
target_link_libraries(acceptor proto anna ${PROTOBUF_LIBRARY})
target_link_libraries(batcher proto anna ${PROTOBUF_LIBRARY})
target_link_libraries(proposer proto anna ${PROTOBUF_LIBRARY})
target_link_libraries(proxy_leader proto anna ${PROTOBUF_LIBRARY})
target_link_libraries(unbatcher proto anna ${PROTOBUF_LIBRARY})