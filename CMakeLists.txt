cmake_minimum_required(VERSION 3.16)
project(pds-cloud-storage)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

include_directories(.)

add_subdirectory(shared)
add_subdirectory(server)
add_subdirectory(client)

