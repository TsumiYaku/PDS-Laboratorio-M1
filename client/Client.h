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
#include <queue>
#include <ios>

#include "FileWatcher.h"
#include <packets/Message.h>
#include <Socket.h>
#include <Checksum.h>
#include <Folder.h>
#include <SimpleCrypt.h>

#include <boost/filesystem.hpp>
#include <boost/serialization/serialization.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/serialization/access.hpp>
#include <boost/serialization/string.hpp>
#define NUM_POSSIBLE_TRY_RESOLVE_ERROR 100

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
    FileStatus prec_status;
    std::string prec_path;
    bool before_first_synch;
    std::queue<std::pair<std::string, FileStatus>> before_first_synch_request;
    int port;
    int contFileToSend = 0;

    void inviaFile(path, FileStatus, bool); //ivia file al server in modalità asincrona o sincrona(conThread = false)
    void downloadDirectory(); //scarica il contenuto inviato dal server fino alla recezione del messagio END
    bool riceviFile();
    //std::string readline(); //legge una riga da command line del client
    void sendMessage(Message&&);
    void sendCreateFileAsynch(std::string path_to_watch);
    void sendEraseFileAsynch(std::string path_to_watch);
    void sendModifyFileAsynch(std::string path_to_watch);

    //void sendMessageWithInfoSerialize(Message &&m);
    Message awaitMessage(size_t, MessageType type = MessageType::text);
    void sendMessageWithResponse(std::string, std::string);


public:
    Client(const Client&) = delete;
    Client();
    Client& operator=(Client&&);
    Client(Client&&);
    Client(std::string , int );
    ~Client();
    
    void close(); //chiude client
    //void recieveACK(Message&& m); //ricezione di un messaggio ACK
    bool doLogin(std::string, std::string ); //effettua login. restituisce true se si è effettuato login da server o false se username è gia stato preso da altro user
    void monitoraCartella(std::string ); //client in connessione con server e in ascolto per backup
};
