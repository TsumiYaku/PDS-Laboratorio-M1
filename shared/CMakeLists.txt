add_library(shared
        Checksum.h
        Checksum.cpp
        Socket.h
        Socket.cpp)

target_include_directories(shared PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})