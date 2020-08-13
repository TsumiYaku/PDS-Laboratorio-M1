add_executable(server
        main.cpp
        ServerSocket.cpp
        ServerSocket.h
        Server.cpp
        Server.h
        AuthManager.cpp
        AuthManager.h)

target_link_libraries(server shared)
find_package(Boost 1.73.0 COMPONENTS filesystem serialization system)
if (Boost_FOUND)
    #add_compile_options(-lboost_system -lboost_filesystem)
    include_directories(${Boost_INCLUDE_DIRS})
    target_link_libraries(server ${Boost_LIBRARIES})
endif ()

target_link_libraries(server sqlite3)
