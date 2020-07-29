cmake_minimum_required(VERSION 3.16)
project(server)

set(CMAKE_CXX_STANDARD 14)

include_directories(.)

find_package(Boost 1.73.0 COMPONENTS filesystem)

if (Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS})
    add_executable(server
            Folder.cpp
            Folder.h
            main.cpp
            ServerSocket.cpp
            ServerSocket.h)
    target_link_libraries(server ${Boost_LIBRARIES})
endif ()

