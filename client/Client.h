#include <iostream>
#include <thread>
#include <mutex>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string>
#include <list>
#include <map>
#include <functional>
#include <shared_mutex>
#include <string.h>
#include <sstream>
#include <optional>
#include <boost/filesystem.hpp>
#include "FileWatcher.h"

using namespace boost::filesystem;

enum ClientStatus{
    start, active, exit
};

class Client{
    int sock;
    struct sockaddr_in saddr;
    ClientStatus status;
    static std::mutex m;
    static std::condition_variable modifica;

    std::string username;
    std::string directory;

    //Server* server;

    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;
    Client(int sock /*, Server* server*/): sock(sock), /*Server(server),*/ status(start){}

    std::string readline(); //legge una riga da command line del client

    void close(); //chiude client
    int inviaFile(std::string path_); //ivia file/directory al server, ritorna status
    void handleConnection(); //client in connessione con server e in ascolto per backup
};
