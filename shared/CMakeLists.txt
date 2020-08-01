add_library(shared
        Checksum.h
        Checksum.cpp
        Socket.h
        Socket.cpp
        packets/Message.cpp
        packets/Message.h
        packets/FileWrapper.cpp
        packets/FileWrapper.h
        packets/main.cpp)

target_include_directories(shared PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
