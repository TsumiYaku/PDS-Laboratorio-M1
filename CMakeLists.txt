cmake_minimum_required(VERSION 3.16)
project(pds-cloud-storage)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "-std=c++17 -pthread -lboost_serialization -lboost_system  -lboost_filesystem")
set(CMAKE_BUILD_TYPE Debug)

add_subdirectory(shared)
#add_subdirectory(server)
add_subdirectory(client)
