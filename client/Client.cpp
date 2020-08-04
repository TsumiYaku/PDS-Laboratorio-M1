#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <thread>
#include <mutex>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <functional>
#include <fcntl.h>
#include <string.h>
#include <stdexcept>

#include "Client.h"
#include <packets/FileWrapper.h>
#include <packets/Message.h>

#include <boost/serialization/serialization.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/serialization/access.hpp>
#include <boost/serialization/string.hpp>

using namespace boost::filesystem;
using Serializer = boost::archive::text_oarchive;
using Deserializer = boost::archive::text_iarchive;

std::mutex mout;
void log(std::string msg){
    std::lock_guard<std::mutex> lg(mout);
    std::cout<<msg<<std::endl;
}

//calcola il checksum partendo dal path
Checksum getChecksum(path p) {
    Checksum checksum = Checksum();
    for(filesystem::path path: p)
        checksum.add(path.string());
    return checksum;
}



/*************************CLIENT********************************************/
Client::Client(int socket, std::string address, int port): address(address), port(port), status(start){
        memset(sad, 0, sizeof(sad));
        sad->sin_family = AF_INET;
        sad->sin_addr.s_addr = inet_addr(address.c_str());
        sad->sin_port = htons(port);
        sock = Socket(socket);
        sock.connect(sad, sizeof(sad));
}

std::string Client::readline(){ //username o directory
     std::string buffer;
     char c;
     while(sock.read(&c, 1, 0)){
        if(c=='\n')
          return buffer;
        else if(c |= '\r') buffer += c;
        }
     return std::string("");
}

void Client::close() /*throw(std::runtime_error)*/{
    status = closed;
    sock.closeSocket();     
}

Client::~Client(){
    close();
}

bool Client::doLogin(std::string user, std::string password){
    Message m = Message("LOGIN_" + user + "_" + password);
    char* textMessage;
    std::stringstream ss;
    Serializer oa2(ss);
    m.serialize(oa2, 0);
    while(true){
        //invio user password
        sock.write(ss.str().c_str(), strlen(ss.str().c_str())+1, 0);//invio richiesta di login a server

        //read response
        sock.read(textMessage, sizeof(textMessage), 0);
        ss << textMessage;
        Deserializer oa(ss);
        m = Message(MessageType::text);
        if(m.getMessage().compare("OK") == 0 || m.getMessage().compare("NOT_OK") == 0) break;
    }
    if(m.getMessage().compare("OK") == 0) //login effettuato con successo
        return true;
    return false; //login non effettuato
}

void Client::monitoraCartella(std::string p){
    std::thread controllaDirectory([this, p]() -> void {
        //std::lock_guard<std::mutex> lg(this->mu);
        path dir(p);
        Message m = Message("SYNC");
        int checksumServer = 0, checksumClient = getChecksum(dir).getChecksum();
        std::stringstream ss;
        Serializer oa(ss);
        m.serialize(oa, 0);
        sock.write(ss.str().c_str(), strlen(ss.str().c_str())+1, 0);//invio richiesta di sincronizzazione a server
        sock.read(&checksumServer, sizeof(checksumServer), 0);//ricevo checksum da server
       
        if(exists(dir)){ //la cartella esiste e quindi la invio al server  
            if((checksumServer == 0 && checksumClient != 0) || checksumClient != checksumServer){//invio tutto la directory per sincornizzare
                //invio richiesta update directory server (fino ad ottenere response ACK
                Message m = Message("UPDATE");
                recieveACK(std::move(m));

                //invio solo i file con checksum diverso
                for(filesystem::path path: dir)
                   inviaFile(path, FileStatus::modified);
                
            }
        }
        else if(checksumServer != 0 && checksumClient==0){ 
                Message m = Message("DOWNLOAD");
                recieveACK(std::move(m));

                downloadDirectory(); //scarico contenuto del server
        }

        //mi metto in ascolto e attendo una modifica
        FileWatcher fw{p, std::chrono::milliseconds(1000)};
        fw.start([this](std::string path_to_watch, FileStatus status) -> void {
            switch(status) {
                case FileStatus::created:{
                    std::cout << "Created: " << path_to_watch << '\n';
                    //invio richiesta creazione file
                    Message m = Message("CREATE");
                    recieveACK(std::move(m));

                    //invio file
                    inviaFile(path(path_to_watch), FileStatus::created); 

                    break;
                }
                case FileStatus::modified:{
                //dico al server che è stato modificato un file e lo invio al server
                    std::cout << "Modified: " << path_to_watch << '\n';

                    //invio richiesta modifica
                    Message m = Message("MODIFY");
                    recieveACK(std::move(m));

                    //invio il file
                    inviaFile(path(path_to_watch), FileStatus::modified);
                   
                    break;
                }
                case FileStatus::erased:{
                //dico al server che è stato modificato un file e lo invio al server
                    std::cout << "Erased: " << path_to_watch << '\n';
                    //invio richiesta cancellazione
                    Message m = Message("ERASE");
                    recieveACK(std::move(m));
                    //invio file
                    inviaFile(path(path_to_watch), FileStatus::erased); 
                }
                case FileStatus::nothing:{
                    //nessuna modifica da effettuare
                    Message m = Message("OK");
                    recieveACK(std::move(m));
                }
            }
  	    }); 
    });

    controllaDirectory.detach();
}


void Client::recieveACK(Message&& m){
   char* textMessage;
   std::stringstream ss;
   Serializer oa(ss);
   m.serialize(oa, 0);
   while(true){
        sock.write(ss.str().c_str(), strlen(ss.str().c_str())+1, 0);//invio richiesta di modifica file a server
        sock.read(textMessage, sizeof(textMessage), 0);
        ss << textMessage;
        Deserializer oa(ss);
        Message m2 = Message(MessageType::text);
        if(m2.getMessage().compare("ACK") == 0) break;
   }
}

void Client::downloadDirectory(){
    std::thread download([this]() -> void{
        //std::lock_guard<std::mutex> lg(this->mu);
        char* textMessage;
        std::stringstream ss;

        while(true){
            //read file (server send file after as soon as send ACK) 
            sock.read(textMessage, sizeof(textMessage), 0);
            ss << textMessage;
            //read file Message or text "TERMINATED" (not using Message wrapper)
            if(ss.str().compare("TERMINATED") == 0) break;

            Message m = Message(MessageType::file); 
            Deserializer ia(ss);
            m.unserialize(ia, 0);
            FileWrapper f = m.getFileWrapper();
            for(filesystem::path path: f.getPath()){
                if(is_directory(path))
                    create_directory(path);
                else if(is_regular_file(path)){
                    std::ofstream output(path.relative_path().string());
                    if(output.bad()) throw std::runtime_error("Error writing file");
                    char* data = strdup(f.getData());
                    output << data;
                    output.close();
                }
            }
            m = Message("ACK");
            Serializer oa(ss);
            m.serialize(oa, 0);
            sock.write(ss.str().c_str(), strlen(ss.str().c_str())+1, 0);//invio ACK al server
        };
    });
    download.detach();
}


void Client::inviaFile(filesystem::path p, FileStatus status) /*throw (std::runtime_error, filesystem::filesystem_error)*/{
    Message m = Message("CHECK");
    recieveACK(std::move(m));
    int checksumServer = 0, checksumClient = getChecksum(p).getChecksum();
    std::stringstream ss;
    Serializer oa(ss);
    m.serialize(oa, 0);
    sock.write(ss.str().c_str(), strlen(ss.str().c_str())+1, 0);//invio richiesta di checksum a server
    sock.read(&checksumServer, sizeof(checksumServer), 0);//ricevo checksum da server
    //invio ACK arrivo checksum
    m = Message("ACK");
    Serializer oa2(ss);
    m.serialize(oa2, 0);
    sock.write(ss.str().c_str(), strlen(ss.str().c_str())+1, 0);//invio ACK al server

    //controllo checksum ed eventuale sincro
    if(checksumClient != checksumServer){
        sincronizzaFile(p.relative_path().string(), status);
    }
}

void Client::sincronizzaFile(std::string path_to_watch, FileStatus status) /*throw(filesystem::filesystem_error, std::runtime_error)*/{
    std::thread synch([this, path_to_watch, status] () -> void {
        //std::lock_guard<std::mutex> lg(this->mu);

        path p(path_to_watch);
        char* response;
        std::stringstream ss;
        while(true){
            if(is_directory(p)){
                //invio directory da sincronizzare
                FileWrapper f = FileWrapper(p, strdup(""), status);
                Message m = Message(std::move(f));
                Serializer oa(ss);
                m.serialize(oa, 0);
                sock.write(ss.str().c_str(), strlen(ss.str().c_str())+1, 0);

                //ricevo response
                sock.read(response, sizeof(response), 0);
                ss << response;
                m = Message(MessageType::text);
                Deserializer ia(ss);
                m.unserialize(ia, 0);

                //invio ACK arrivo checksum
                Message m2 = Message("ACK");
                Serializer oa2(ss);
                m2.serialize(oa2, 0);
                sock.write(ss.str().c_str(), strlen(ss.str().c_str())+1, 0);//invio ACK al server

                //leggo messaggio da server
                if(m.getMessage().compare("ACK") == 0) break;
            }else if(is_regular_file(p)){
                //invio file da sincronizzare
                std::ifstream input(p.relative_path().string());
                if(input.bad()) {
                    Message m2 = Message("ERROR_FILE");
                    Serializer oa2(ss);
                    m2.serialize(oa2, 0);
                    sock.write(ss.str().c_str(), strlen(ss.str().c_str())+1, 0);//invio error_File al server
                    throw std::runtime_error("Error reading file"); 
                }
    
                std::stringstream sstr;
                int size=0;
                while(input >> sstr.rdbuf()){
                    size += sstr.str().length();
                } //read all content of file
                input.close();

                char* data = strdup(sstr.str().c_str());
                FileWrapper f = FileWrapper(p, data, status);
                Message m = Message(std::move(f));
                Serializer oa2(ss);
                m.serialize(oa2, 0);
                sock.write(ss.str().c_str(), strlen(ss.str().c_str())+1, 0); 

                //ricevo response
                sock.read(response, sizeof(response), 0);
                ss << response;
                m = Message(MessageType::text);
                Deserializer ia2(ss);
                m.unserialize(ia2, 0);

                //invio ACK arrivo checksum
                Message m2 = Message("ACK");
                Serializer oa3(ss);
                m2.serialize(oa3, 0);
                sock.write(ss.str().c_str(), strlen(ss.str().c_str())+1, 0);//invio ACK al server

                //leggo ACK
                if(m.getMessage().compare("ACK") == 0) break;
            }
            else {
                Message m2 = Message("ERROR_FILE");
                Serializer oa2(ss);
                m2.serialize(oa2, 0);
                sock.write(ss.str().c_str(), strlen(ss.str().c_str())+1, 0);//invio ERROR_FILE al server
                throw std::runtime_error("file not supported");
            }
        }
        //ho finito di inviare file
        Message m = Message("END");
        recieveACK(std::move(m));
    });
    synch.detach();
}

