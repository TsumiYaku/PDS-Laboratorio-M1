cmake_minimum_required(VERSION 3.16)
project(pds-cloud-storage)

set(CMAKE_CXX_STANDARD 17)

include_directories(.)

add_subdirectory(shared)
add_subdirectory(server)