add_library(shared
        Checksum.h
        Checksum.cpp
        Socket.h
        Socket.cpp
        communication/Message.cpp
        communication/Message.h
        communication/FileWrapper.cpp
        communication/FileWrapper.h
        SimpleCrypt.h
        SimpleCrypt.cpp
        Folder.cpp
        Folder.h communication/Exchanger.cpp communication/Exchanger.h)

target_include_directories(shared PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
