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
#include <Checksum.h>
#include <boost/serialization/serialization.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/serialization/access.hpp>
#include <boost/serialization/string.hpp>

using namespace boost::filesystem;
using Serializer = boost::archive::text_oarchive;
using Deserializer = boost::archive::text_iarchive;

enum ClientStatus{
    start, active, closed
};

class Client{

    Socket sock;
    ClientStatus status;
    static std::mutex mu;

    //std::string username;
    //std::string password;

    //std::string directory;

    struct sockaddr_in* sad;
    std::string address;
    int port;
    
    void inviaFile(path path_, FileStatus status); //ivia richiesta aggiunta/modifica file al server
    void downloadDirectory(); //scarica il contenuto inviato dal server fino alla recezione del messagio END
    std::string readline(); //legge una riga da command line del client
    void sincronizzaFile(std::string path_, FileStatus status); //invia directory/file modificata al server in modalità asincrona (thread separato)
    
public:
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;
    Client(int sock, std::string address, int port);
    ~Client();
    
    void close(); //chiude client
    bool doLogin(std::string user, std::string password); //effettua login. restituisce true se si è effettuato logi da server o false se user o psw è errata
    void monitoraCartella(std::string path); //client in connessione con server e in ascolto per backup
};
