add_executable(client
        Client.h
        Client.cpp
        FileWatcher.h
        FileWatcher.cpp
        )

target_link_libraries(client shared)
find_package(Boost 1.73.0 COMPONENTS filesystem)
if (Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS})
    target_link_libraries(client ${Boost_LIBRARIES})
endif ()
