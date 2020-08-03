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

void Client::close(){
    status = closed;
    sock.closeSocket();
     //this->server->disattivaClient(this->sock);
     
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
    do{
        sock.write(ss.str().c_str(), strlen(ss.str().c_str())+1, 0);//invio richiesta di login a server
        sock.read(textMessage, sizeof(textMessage), 0);
        ss << textMessage;
    }while(ss.str() != "OK" && ss.str() != "NOT_OK");
    if(ss.str() == "OK") //login effettuato con successo
        return true;
    return false; //login non effettuato
}

void Client::monitoraCartella(std::string p){
    std::thread controllaDirectory([this, p]() -> void {
        //std::lock_guard<std::mutex> lg(mu);
        path dir(p);
        Message m = Message("SYNC");
        int checksumServer = 0, checksumClient = getChecksum(dir).getChecksum();
        std::stringstream ss;
        Serializer oa(ss);
        m.serialize(oa, 0);
        sock.write(ss.str().c_str(), strlen(ss.str().c_str())+1, 0);//invio richiesta di sincornizzazione a server
        sock.readInt(&checksumServer, sizeof(checksumServer), 0);//ricevo checksum da server
       
        if(exists(dir)){ //la cartella esiste e quindi la invio al server  
            if((checksumServer == 0 && checksumClient != 0) || checksumClient != checksumServer){//invio tutto la directory per sincornizzare
                //invio richiesta update directory server (fino ad ottenere response ACK)
                Message m2 = Message("UPDATE");
                char* textMessage;
                Serializer oa2(ss);
                m.serialize(oa2, 0);
                do{
                    sock.write(ss.str().c_str(), strlen(ss.str().c_str())+1, 0);//invio richiesta di aggiornamento a server
                    sock.read(textMessage, sizeof(textMessage), 0);
                    ss << textMessage;
                }while(ss.str() != "ACK");


                //invio solo i file con checksum diverso
                for(filesystem::path path: dir)
                   inviaFile(path, FileStatus::modified);
                
            }
        }
        else if(checksumServer != 0 && checksumClient==0){
                Message m2 = Message("DOWNLOAD");
                char* textMessage;
                Serializer oa2(ss);
                m.serialize(oa2, 0);
                do{
                    sock.write(ss.str().c_str(), strlen(ss.str().c_str())+1, 0);//invio richiesta di download a server
                    sock.read(textMessage, sizeof(textMessage), 0);
                    ss << textMessage;
                }while(ss.str() != "ACK");

                downloadDirectory(); //scarico contenuto del server
        }

        //mi metto in ascolto e attendo una modifica
        FileWatcher fw{p, std::chrono::milliseconds(1000)};
        fw.start([this](std::string path_to_watch, FileStatus status) -> void {
            int ret;
            switch(status) {
                case FileStatus::created:{
                    std::cout << "Created: " << path_to_watch << '\n';

                    //invio richiesta creazione file
                    std::stringstream ss;
                    Message m = Message("CREATE");
                    char* textMessage;
                    Serializer oa(ss);
                    m.serialize(oa, 0);
                    do{
                        sock.write(ss.str().c_str(), strlen(ss.str().c_str())+1, 0);//invio richiesta di aggiunta file a server
                        sock.read(textMessage, sizeof(textMessage), 0);
                        ss << textMessage;
                    }while(ss.str() != "ACK");

                    //invio file
                    inviaFile(path(path_to_watch), FileStatus::created); 

                    break;
                }
                case FileStatus::modified:{
                //dico al server che è stato modificato un file e lo invio al server
                    std::cout << "Modified: " << path_to_watch << '\n';
                    std::stringstream ss;

                    //invio richiesta modifica
                    Message m = Message("MODIFY");
                    char* textMessage;
                    Serializer oa(ss);
                    m.serialize(oa, 0);
                    do{
                        sock.write(ss.str().c_str(), strlen(ss.str().c_str())+1, 0);//invio richiesta di modifica file a server
                        sock.read(textMessage, sizeof(textMessage), 0);
                        ss << textMessage;
                    }while(ss.str() != "ACK");

                    //invio il file
                    inviaFile(path(path_to_watch), FileStatus::modified);
                   
                    break;
                }
                case FileStatus::erased:{
                //dico al server che è stato modificato un file e lo invio al server
                    std::cout << "Erased: " << path_to_watch << '\n';

                    //invio richiesta cancellazione
                    std::stringstream ss;
                    Message m = Message("ERASE");
                    char* textMessage;
                    Serializer oa(ss);
                    m.serialize(oa, 0);
                    do{
                        sock.write(ss.str().c_str(), strlen(ss.str().c_str())+1, 0);//invio richiesta di eliminazione file a server
                        sock.read(textMessage, sizeof(textMessage), 0);
                        ss << textMessage;
                    }while(ss.str() != "ACK");
                    
                    //invio file
                    inviaFile(path(path_to_watch), FileStatus::erased); 

                }
            }
  	    }); 
    });

    controllaDirectory.detach();
}

void Client::downloadDirectory(){
    std::thread download([this]() -> void{
        //std::lock_guard<std::mutex> lg(mu);
        char* textMessage;
        std::stringstream ss;
        do{ 
            Message m = Message(MessageType::file); 
            Deserializer ia(ss);
            m.unserialize(ia, 0);
            FileWrapper f = m.getFileWrapper();
            for(filesystem::path path: f.getPath()){
                if(is_directory(path))
                create_directory(path);
                else if(is_regular_file(path)){
                    std::ofstream output(path.relative_path().string());
                    char* data = strdup(f.getData());
                    output << data;
                    output.close();
                }
            }
            m = Message("ACK");
            char* textMessage;
            Serializer oa(ss);
            m.serialize(oa, 0);
            sock.write(ss.str().c_str(), strlen(ss.str().c_str())+1, 0);//invio ACK al server
            sock.read(textMessage, sizeof(textMessage), 0);
            ss << textMessage;
        }while(ss.str() != "END");
    });
    download.detach();
}
void Client::inviaFile(filesystem::path p, FileStatus status){
    Message m = Message("CHECK");
    int checksumServer = 0, checksumClient = getChecksum(p).getChecksum();
    std::stringstream ss;
    Serializer oa(ss);
    m.serialize(oa, 0);
    sock.write(ss.str().c_str(), strlen(ss.str().c_str())+1, 0);//invio richiesta di checsum a server
    sock.readInt(&checksumServer, sizeof(checksumServer), 0);//ricevo checksum da server
    if(checksumClient != checksumServer){
        sincronizzaFile(p.relative_path().string(), status);
    }
}

void Client::sincronizzaFile(std::string path_to_watch, FileStatus status) {
    std::thread synch( [this, path_to_watch, status] () -> void {
        //std::lock_guard<std::mutex> lg(mu);

        path p(path_to_watch);
        char* response;
        do{
            if(is_directory(p)){
                FileWrapper f = FileWrapper(p, strdup(""), status);
                Message m = Message(f);
                std::stringstream ss;
                Serializer oa(ss);
                m.serialize(oa, 0);
                sock.write(ss.str().c_str(), strlen(ss.str().c_str())+1, 0);//invio file da sincornizzare
                sock.read(response, sizeof(response), 0);
            }else if(is_regular_file(p)){
                std::ifstream input(p.relative_path().string());
                std::stringstream sstr;
                while(input >> sstr.rdbuf()){} //read all content of file
                input.close();
                char* data = strdup(sstr.str().c_str());
                FileWrapper f = FileWrapper(p, data, status);
                Message m = Message(f);
                std::stringstream ss;
                Serializer oa(ss);
                sock.write(ss.str().c_str(), strlen(ss.str().c_str())+1, 0); 
                sock.read(response, sizeof(response), 0);
            }
        }while(strcmp(response,"ACK")!=0);
        //ho finito di inviare file
        std::stringstream ss;
        Message m = Message("END");
        char* textMessage;
        Serializer oa(ss);
        m.serialize(oa, 0);
        do{
            sock.write(ss.str().c_str(), strlen(ss.str().c_str())+1, 0);
            sock.read(textMessage, sizeof(textMessage), 0);
            ss << textMessage;
        }while(ss.str() != "ACK");
    });
    synch.detach();
}

