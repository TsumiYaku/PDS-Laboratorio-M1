#pragma once

#include <iostream>
#include <thread>
#include <mutex>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string>
#include <functional>
#include <shared_mutex>
#include <string.h>
#include <sstream>
#include <optional>
#include <boost/filesystem.hpp>
#include "FileWatcher.h"
#include <Socket.h>
#include "../server/ServerSocket.h"

using namespace boost;

enum ClientStatus{
    start, active, closed
};

class Client{

    Socket sock;
    ClientStatus status;
    static std::mutex m;

    std::string username;
    std::string directory;

    struct sockaddr_in* sad;
    std::string address;
    int port;

public:
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;
    Client(int sock , std::string address, int port);
    ~Client();

    std::string readline(); //legge una riga da command line del client

    void close(); //chiude client
    int inviaFile(std::string path_); //ivia file/directory al server, ritorna status
    void handleConnection(); //client in connessione con server e in ascolto per backup
};
