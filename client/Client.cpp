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

void Client::sendMessage(Message &&m) {
    // Serialization
    std::stringstream sstream;
    Serializer oa(sstream);
    m.serialize(oa, 0);

    // Socket write
    std::string s(sstream.str());
    sock.write(s.c_str(), sizeof(s.c_str()), 0);
}

Message&& Client::awaitMessage(size_t msg_size = 1024) {
    // Socket read
    char buf[msg_size];
    int size = sock.read(buf, sizeof(buf), 0);

    // Unserialization
    std::stringstream sstream;
    sstream << buf;
    Deserializer ia(sstream);
    Message m(MessageType::text);
    m.unserialize(ia, 0);

    return std::move(m);
}

/*std::string Client::readline(){ //username o directory
     std::string buffer;
     char c;
     while(sock.read(&c, 1, 0)){
        if(c=='\n')
          return buffer;
        else if(c |= '\r') buffer += c;
        }
     return std::string("");
}*/

void Client::close() /*throw(std::runtime_error)*/{
    status = closed;
    sock.closeSocket();     
}

Client::~Client(){
    close();
}

bool Client::doLogin(std::string user, std::string password){
   
    Message m = Message(MessageType::text);
    while(true){
        m = Message("LOGIN_" + user + "_" + password);
        sendMessage(std::move(m));

        //read response
        m = awaitMessage();

        if(m.getMessage().compare("OK") == 0 || m.getMessage().compare("NOT_OK") == 0) break;
    }
    if(m.getMessage().compare("OK") == 0) //login effettuato con successo
        return true;
    return false; //login non effettuato
}

void Client::monitoraCartella(std::string p){
    std::thread controllaDirectory([this, p]() -> void {
        std::lock_guard<std::mutex> lg(this->mu);
        path dir(p);

        while(true){
            Message m = Message("SYNC");
            sendMessage(std::move(m));

            //read response
            m = awaitMessage();

            if(m.getMessage().compare("ACK") == 0) break;
        }
        
        int checksumServer = 0, checksumClient = getChecksum(dir).getChecksum();
        
        sock.read(&checksumServer, sizeof(checksumServer), 0);//ricevo checksum da server
       
        if(exists(dir)){ //la cartella esiste e quindi la invio al server  
            if((checksumServer == 0 && checksumClient != 0) || checksumClient != checksumServer){//invio tutta la directory per sincornizzare
                //invio richiesta update directory server (fino ad ottenere response ACK)
                while(true){
                    Message m = Message("UPDATE");
                    sendMessage(std::move(m));

                    //read response
                    m = awaitMessage();

                    if(m.getMessage().compare("ACK") == 0) break;
                }

                //invio solo i file con checksum diverso
                for(filesystem::path path: dir)
                   inviaFile(path, FileStatus::modified);
                
            }
        }
        else if(checksumServer != 0 && checksumClient==0){ 
            while(true){
                Message m = Message("DOWNLOAD");
                sendMessage(std::move(m));

                //read response
                m = awaitMessage();

                if(m.getMessage().compare("ACK") == 0) break;
            }

            downloadDirectory(); //scarico contenuto del server
        }

        //mi metto in ascolto e attendo una modifica
        FileWatcher fw{p, std::chrono::milliseconds(1000)};
        fw.start([this](std::string path_to_watch, FileStatus status) -> void {
            switch(status) {
                case FileStatus::created:{
                    std::cout << "Created: " << path_to_watch << '\n';
                    //invio richiesta creazione file
                    while(true){
                        Message m = Message("CREATE");
                        sendMessage(std::move(m));

                        //read response
                        m = awaitMessage();

                        if(m.getMessage().compare("ACK") == 0) break;
                    }

                    //invio file
                    inviaFile(path(path_to_watch), FileStatus::created); 

                    break;
                }
                case FileStatus::modified:{
                //dico al server che è stato modificato un file e lo invio al server
                    std::cout << "Modified: " << path_to_watch << '\n';

                    //invio richiesta modifica
                    while(true){
                        Message m = Message("MODIFY");
                        sendMessage(std::move(m));

                        //read response
                        m = awaitMessage();

                        if(m.getMessage().compare("ACK") == 0) break;
                    }           

                    //invio il file
                    inviaFile(path(path_to_watch), FileStatus::modified);
                   
                    break;
                }
                case FileStatus::erased:{
                //dico al server che è stato modificato un file e lo invio al server
                    std::cout << "Erased: " << path_to_watch << '\n';
                    //invio richiesta cancellazione
                    while(true){
                        Message m = Message("ERASE");
                        sendMessage(std::move(m));

                        //read response
                        m = awaitMessage();

                        if(m.getMessage().compare("ACK") == 0) break;
                    }
                    //invio file
                    inviaFile(path(path_to_watch), FileStatus::erased); 
                }
                case FileStatus::nothing:{
                    //nessuna modifica da effettuare
                    Message m = Message("OK");
                    sendMessage(std::move(m));

                }
            }
  	    }); 
    });

    controllaDirectory.detach();
}


void Client::downloadDirectory(){
    std::thread download([this]() -> void{
        std::lock_guard<std::mutex> lg(this->mu);
        char* textMessage;
        std::stringstream ss;
        char buf[1024];
        while(true){
            //read message (server send file as soon as sending ACK)  
            
            int size = sock.read(buf, sizeof(buf), 0);
            if (strcmp(buf,"END") == 0) break; //send END in chiaro (non serializato) per riconoscere il tipo di messaggio
            
            // Unserialization
            std::stringstream sstream;
            sstream << buf;
            Deserializer ia(sstream);
            Message m(MessageType::file);
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
            sendMessage(std::move(m));
        };
    });
    download.detach();
}


void Client::inviaFile(filesystem::path p, FileStatus status) /*throw (std::runtime_error, filesystem::filesystem_error)*/{
    while(true){
        Message m = Message("CHECK");
        sendMessage(std::move(m));
        m = awaitMessage();
        if(m.getMessage().compare("ACK") == 0) break;
    }

    int checksumServer = 0, checksumClient = getChecksum(p).getChecksum();

    sock.read(&checksumServer, sizeof(checksumServer), 0);//ricevo checksum intero da server

    //invio ACK all'arrivo checksum
    Message m = Message("ACK");
    sendMessage(std::move(m));//invio ACK al server

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
                sendMessage(std::move(m));

                //ricevo response
                m = awaitMessage();
               
                Message m2 = Message("ACK");
                sendMessage(std::move(m2));

                //leggo messaggio da server
                if(m.getMessage().compare("ACK") == 0) break;

            }else if(is_regular_file(p)){
                //invio file da sincronizzare
                std::ifstream input(p.relative_path().string());
                if(input.bad()) {
                    Message m = Message("ERR");
                    sendMessage(std::move(m));
                    throw std::runtime_error("Error reading file"); 
                }
    
                std::stringstream sstr;
                int size=0;
                while(input >> sstr.rdbuf()){
                    size += sstr.str().length();
                } 
                input.close();

                char* data = strdup(sstr.str().c_str());
                FileWrapper f = FileWrapper(p, data, status);
                Message m = Message(std::move(f));
                sendMessage(std::move(m));

                //ricevo response
                m = awaitMessage();

                //invio ACK arrivo checksum
                Message m2 = Message("ACK");
                sendMessage(std::move(m2));

                //leggo ACK
                if(m.getMessage().compare("ACK") == 0) break;
            }
            else {
                Message m2 = Message("ERR");
                sendMessage(std::move(m2));
                throw std::runtime_error("file not supported");
            }
        }
        //ho finito di inviare file
        while(true){
            Message m = Message("END");
            sendMessage(std::move(m));
            m = awaitMessage();
            if(m.getMessage().compare("ACK") == 0) break;
        }
    });
    synch.detach();
}
