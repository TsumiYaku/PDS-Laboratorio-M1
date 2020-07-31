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
#include <Checksum.h>

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
    int inviaFile(std::string path_); //ivia file al server, ritorna stato
    void inviaDirectory(path dir); //ivia directory e suo contenuto al server
    std::string readline(); //legge una riga da command line del client
    void sincronizza(std::string path_); //invia directory/file modificata a server fino ad essere sinconizzata con quella del client
public:
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;
    Client(int sock, std::string address, int port);
    ~Client();
    

    void close(); //chiude client
    
    void monitoraCartella(); //client in connessione con server e in ascolto per backup
};
