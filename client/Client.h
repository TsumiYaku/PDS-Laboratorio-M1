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
    Socket sock; //socket client
    std::mutex mu, mu2, muSend; //mutex per gestione mutua esclusione
    std::string user; //nome utente
    struct sockaddr_in* sad; 
    std::string address;//indirizzo server
    int cont_error; //conta il numero di errori dovuti a problemi di runtime, rete, filestem
    Folder* directory; //directory da monitorare
    FileStatus prec_status; //stato del file: creato, modificato o cancellato
    std::map<std::string, FileStatus> request; //coda delle richieste da inviare al client
    int port;
    int cont_nothing = 0;

    
    //comunication
    void sendMessage(Message&&); //invia un messaggio al server
    Message awaitMessage(size_t, MessageType type = MessageType::text);//attende un messaggio. di default un testo ma potrebbe essere anche un FileWrapper (contenente le info di un file)
    void sendMessageWithResponse(std::string, std::string);//invio un messaggio fino alla recezione di un ACK
    
    //request
    void sendCreateFileAsynch(std::string path_to_watch);//invia il file con stato creato al server su un thread separato
    void sendEraseFileAsynch(std::string path_to_watch);//invia il file con stato cancellato al server su un thread separato
    void sendModifyFileAsynch(std::string path_to_watch);//invia il file con stato modificato al server su un thread separato
    void sendAllRequest(FileWatcher& fw, bool first_syncro = false);//invia tutte le richieste accoate al server
    void sendWrapperAllRequest(FileWatcher& fw, bool first_syncro = false);//wrapper di sendAllRequest
    void downloadDirectory(); //scarica il contenuto inviato dal server fino alla recezione del messagio END
    
public:
    Client(const Client&) = delete;
    Client();
    Client(std::string , int );
    ~Client();
    
    void close(); //chiude il socket client
    bool doLogin(std::string, std::string ); //effettua login. restituisce true se si è effettuato correttamente login false altrimenti
    void monitoraCartella(std::string ); //client in connessione con server e in ascolto per backup
};
