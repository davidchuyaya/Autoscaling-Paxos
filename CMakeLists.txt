cmake_minimum_required(VERSION 3.17 FATAL_ERROR)
project(Autoscaling_Paxos)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED on)

IF(${CMAKE_CXX_COMPILER} STREQUAL "/usr/bin/clang++")
    SET(CMAKE_CXX_FLAGS_COMMON
            "-std=c++17 \
      -stdlib=libc++ -pthread")
ENDIF()

IF(${CMAKE_CXX_COMPILER} STREQUAL "/usr/bin/g++")
    SET(CMAKE_CXX_FLAGS_COMMON
            "-std=c++17 -pthread")
ENDIF()

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

ADD_SUBDIRECTORY(include/prometheus-cpp)
SET(ENABLE_TESTING OFF)
SET(prometheus-cpp_DIR include/prometheus-cpp/_build/deploy/usr/local/lib/cmake/prometheus-cpp)
FIND_PACKAGE(prometheus-cpp CONFIG REQUIRED)

set(PROXY_LEADERS_SRC proxy_leader.cpp proxy_leader.hpp)
set(BATCHERS_SRC batcher.cpp batcher.hpp)
set(PROPOSER_SRC proposer.cpp proposer.hpp)
set(ACCEPTOR_SRC acceptor.cpp acceptor.hpp)
set(UNBATCHER_SRC unbatcher.cpp unbatcher.hpp)
set(MAIN_SRC main.cpp main.hpp)
set(MOCK_COMPONENT_SRC mock_component.cpp mock_component.hpp)
file(GLOB UTILS_SRC "utils/*.hpp" "utils/*.cpp")
file(GLOB MODELS_SRC "models/*.hpp" "models/*.cpp")
file(GLOB STORAGE_SRC "lib/storage/*.hpp" "lib/storage/*.cpp")
file(GLOB AWS_SRC "aws/*.hpp" "aws/*.cpp")
file(GLOB MOCK_SRC "mock/*.hpp" "mock/*.cpp")

include_directories(${CMAKE_CURRENT_BINARY_DIR})

set(SRCS ${UTILS_SRC} ${STORAGE_SRC} ${MODELS_SRC})
add_executable(Autoscaling_Paxos ${MAIN_SRC} ${SRCS} ${AWS_SRC})
add_executable(acceptor ${ACCEPTOR_SRC} ${SRCS})
add_executable(batcher ${BATCHERS_SRC} ${SRCS})
add_executable(proposer ${PROPOSER_SRC} ${SRCS})
add_executable(proxy_leader ${PROXY_LEADERS_SRC} ${SRCS})
add_executable(unbatcher ${UNBATCHER_SRC} ${SRCS})
add_executable(mock_component ${MOCK_COMPONENT_SRC} ${SRCS} ${MOCK_SRC})

set(LIBRARIES proto hydro-zmq zmq yaml-cpp pthread prometheus-cpp::pull ${PROTOBUF_LIBRARY})
target_link_libraries(Autoscaling_Paxos ${LIBRARIES})
target_link_libraries(acceptor ${LIBRARIES})
target_link_libraries(batcher ${LIBRARIES})
target_link_libraries(proposer ${LIBRARIES})
target_link_libraries(proxy_leader ${LIBRARIES})
target_link_libraries(unbatcher ${LIBRARIES})
target_link_libraries(mock_component ${LIBRARIES})

set(DEPENDENCIES hydro-zmq zeromq zeromqcpp)
add_dependencies(Autoscaling_Paxos ${DEPENDENCIES})
add_dependencies(acceptor ${DEPENDENCIES})
add_dependencies(batcher ${DEPENDENCIES})
add_dependencies(proposer ${DEPENDENCIES})
add_dependencies(proxy_leader ${DEPENDENCIES})
add_dependencies(unbatcher ${DEPENDENCIES})
add_dependencies(mock_component ${DEPENDENCIES})
