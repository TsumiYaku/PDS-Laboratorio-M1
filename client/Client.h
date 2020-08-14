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
#include <map>
#include <ios>
#include <condition_variable>

#include "FileWatcher.h"
#include <communication/Message.h>
#include <Socket.h>
#include <Checksum.h>
#include <Folder.h>

#include <boost/filesystem.hpp>
#include <boost/serialization/serialization.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/serialization/access.hpp>
#include <boost/serialization/string.hpp>
#define NUM_POSSIBLE_TRY_RESOLVE_ERROR 100
#define TIME_MONITORING 5000

using namespace boost::filesystem;
using Serializer = boost::archive::text_oarchive;
using Deserializer = boost::archive::text_iarchive;

class Client{
    Socket sock;
    std::mutex mu, mu2, muSend;
    std::string user;
    struct sockaddr_in* sad;
    std::string address;
    int cont_error;
    Folder* directory;
    FileStatus prec_status;
    std::string prec_path;
    bool before_first_synch;
    std::map<std::string, FileStatus> request;
    int port;
    int cont_nothing = 0;
    //std::condition_variable cv_request;

    int contFileToSend = 0;

    void downloadDirectory(); //scarica il contenuto inviato dal server fino alla recezione del messagio END
    //std::string readline(); //legge una riga da command line del client
    void sendMessage(Message&&);
    void sendCreateFileAsynch(std::string path_to_watch);
    void sendEraseFileAsynch(std::string path_to_watch);
    void sendModifyFileAsynch(std::string path_to_watch);

    //void sendMessageWithInfoSerialize(Message &&m);
    Message awaitMessage(size_t, MessageType type = MessageType::text);
    void sendMessageWithResponse(std::string, std::string);
    void sendAllRequest(FileWatcher& fw, bool first_syncro = false);
    void sendWrapperAllRequest(FileWatcher& fw, bool first_syncro = false);

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
