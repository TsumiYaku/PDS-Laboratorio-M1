#pragma once

#include <iostream>
#include <thread>
#include <mutex>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string>
#include <functional>
#include <string.h>
#include <sstream>
#include <optional>

#include "FileWatcher.h"
#include <packets/Message.h>
#include <Socket.h>
#include <Checksum.h>
#include <Folder.h>

#include <boost/filesystem.hpp>
#include <boost/serialization/serialization.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/serialization/access.hpp>
#include <boost/serialization/string.hpp>
#define NUM_POSSIBLE_TRY_RESOLVE_ERROR 1000

using namespace boost::filesystem;
using Serializer = boost::archive::text_oarchive;
using Deserializer = boost::archive::text_iarchive;

class Client{

    Socket sock;
    std::mutex mu;
    std::string user;
    struct sockaddr_in* sad;
    std::string address;
    int cont_error;
    Folder* directory;

    int port;
    
    void inviaFile(path path_, FileStatus status); //ivia richiesta aggiunta/modifica file al server
    void downloadDirectory(); //scarica il contenuto inviato dal server fino alla recezione del messagio END
    //std::string readline(); //legge una riga da command line del client
    void sincronizzaFile(path path_, FileStatus status); //invia directory/file modificata al server in modalità asincrona (thread separato)
    void sendMessage(Message&&);
    Message awaitMessage(size_t);
    void sendMessageWithResponse(std::string message, std::string response);


public:
    Client(const Client&) = delete;

    Client& operator=(Client&&);
    Client(Client&&);
    Client(std::string address, int port);
    ~Client();
    
    void close(); //chiude client
    //void recieveACK(Message&& m); //ricezione di un messaggio ACK
    bool doLogin(std::string user); //effettua login. restituisce true se si è effettuato login da server o false se username è gia stato preso da altro user
    void monitoraCartella(std::string path); //client in connessione con server e in ascolto per backup
};
