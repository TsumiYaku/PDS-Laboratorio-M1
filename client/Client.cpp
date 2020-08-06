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
#include <ios>

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



/*************************CLIENT********************************************/
Client::Client(std::string address, int port): address(address), port(port), status(start){
        struct sockaddr_in sockaddrIn;
        sockaddrIn.sin_port = ntohs(port);
        sockaddrIn.sin_family = AF_INET;
        if(::inet_pton(AF_INET, address.c_str(), &sockaddrIn.sin_addr) <=0)
            throw std::runtime_error("error inet_ptn");

        sock.connect(&sockaddrIn, sizeof(sockaddrIn));
}

Client::Client(Client &&other) {

    this->sock = std::move(sock);
    this->status = other.status;
    this->sad = other.sad;
    this->address = std::move(other.address);
    this->port = other.port;
    other.sock.closeSocket();

    std::cout <<"CLIENT MOVE"<<std::endl;
}

Client &Client::operator=(Client &&other) {
    if(this->user != other.user){
        this->sock = std::move(sock);
        this->status = other.status;
        this->sad = other.sad;
        this->address = std::move(other.address);
        this->port = other.port;
        other.sock.closeSocket();
        std::cout <<"CLIENT MOVE OPERETOR="<<std::endl;
    }
    return *this;
}


std::vector<filesystem::path> Client::getContent(path dir) {
    std::vector<filesystem::path> v;

    // Recursive research inside the folder
    //v.push_back(dir);
    for(filesystem::directory_entry& d : filesystem::recursive_directory_iterator(dir))
    {
        v.push_back(strip_root(d.path()));
    }
    return v;
}

//calcola il checksum partendo dal path
uint32_t Client::getChecksum(path p) {
    Checksum checksum = Checksum();
    if(exists(p)){
        for(filesystem::path path : this->getContent(p)){
            //std::cout << path.string() << std::endl;
            checksum.add(path.string());
        }
        return checksum.getChecksum();
    }
    return 0;
}

void Client::sendMessage(Message &&m) {
    // Serialization
    std::stringstream sstream;
    Serializer oa(sstream);
    m.serialize(oa, 0);

    // Socket write
    std::string s(sstream.str());
    log("Message send:" + m.getMessage());
    sock.write(s.c_str(), strlen(s.c_str())+1, 0);
}

Message Client::awaitMessage(size_t msg_size = 1024) {
    // Socket read
    char buf[msg_size];
    int size = sock.read(buf, msg_size, 0);
    //std::cout << buf << " " << size << std::endl;
    // Unserialization
    std::stringstream sstream;
    sstream << buf;
    Deserializer ia(sstream);
    Message m(MessageType::text);
    m.unserialize(ia, 0);
    log("Message recieve:" + m.getMessage());
    return m;
}


void Client::close() /*throw(std::runtime_error)*/{
    status = closed;
    sock.closeSocket();
}

Client::~Client(){
    close();
}

bool Client::doLogin(std::string user, std::string password){
    this->user = user;
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
    //sock.print();

    //std::thread controllaDirectory([this, p](Socket sock) -> void {

        //std::lock_guard<std::mutex> lg(this->mu);
        //sock.print();
        dir = path(p);
        std::cout <<"MONITORING " << dir.string() << std::endl;
        Message m = Message("SYNC");
        sendMessage(std::move(m));

        int checksumServer=0;
        int checksumClient = (int)getChecksum(dir);

        sock.read(&checksumServer, sizeof(checksumServer), 0);//ricevo checksum da server
        std::cout << "CHECKSUM CLIENT " << checksumClient <<std::endl;
        std::cout << "CHECKSUM SERVER " << checksumServer <<std::endl;

        if(exists(dir)){ //la cartella esiste e quindi la invio al server
            if((checksumServer == 0 && checksumClient != 0) || (checksumClient != checksumServer)){//invio tutta la directory per sincornizzare
                //invio richiesta update directory server (fino ad ottenere response ACK)
                while(true){
                    Message m = Message("UPDATE");

                    sendMessage(std::move(m));

                    //read response
                    m = awaitMessage();

                    if(m.getMessage().compare("ACK") == 0) break;
                }

                //invio solo i file con checksum diverso
                 for(filesystem::path path: this->getContent(dir)){

                   std::cout << "INVIO DI: " <<path <<std::endl;
                   inviaFile(path, FileStatus::modified);
                 }

                 while(true){
                     Message m = Message("END");
                     sendMessage(std::move(m));
                     m = awaitMessage();
                    if(m.getMessage().compare("OK") == 0) break;
                 }
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

        }else{
            Message m = Message("OK");
            sendMessage(std::move(m));
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

                    while(true){
                        Message m = Message("END");
                        sendMessage(std::move(m));
                        m = awaitMessage();
                        if(m.getMessage().compare("OK") == 0) break;
                    }
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

                    while(true){
                        Message m = Message("END");
                        sendMessage(std::move(m));
                        m = awaitMessage();
                        if(m.getMessage().compare("OK") == 0) break;
                    }
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
                    while(true){
                        Message m = Message("END");
                        sendMessage(std::move(m));
                        m = awaitMessage();
                        if(m.getMessage().compare("OK") == 0) break;
                    }
                }
                case FileStatus::nothing:{
                    break;
                    //nessuna modifica da effettuare
                    //Message m = Message("OK");
                    //sendMessage(std::move(m));
                }
                default:break;
            }
  	    });
    //}, std::move(sock));

    //controllaDirectory.detach();
}


void Client::downloadDirectory(){
    //std::thread download([this]() -> void{
        //std::lock_guard<std::mutex> lg(this->mu);
        std::stringstream ss;
        char buf[1024];
        while(true){
            //read FILE , DIRECTORY, TERMINATED or FS_ERR
            Message m = awaitMessage();
            sendMessage(Message("OK"));
            if(m.getMessage().compare("END") == 0) break;
            if(m.getMessage().compare("FS_ERR") == 0) break; //TODO

            if(m.getMessage().compare("DIRECTORY") == 0){
                sock.read(buf, 1024, 0);
                ss << buf;
                // Unserialization
                Deserializer ia(ss);
                m = Message(MessageType::file);
                m.unserialize(ia, 0);
                FileWrapper f = m.getFileWrapper();
                create_directories(f.getPath().relative_path());
            }
            else{
                sock.read(buf, 1024, 0);
                ss << buf;
                // Unserialization
                Deserializer ia(ss);
                m = Message(MessageType::file);
                m.unserialize(ia, 0);
                FileWrapper f = m.getFileWrapper();
                filesystem::ofstream file;
                file.open(f.getPath().relative_path(), std::ios::out | std::ios::trunc | std::ios::binary);
                file.write(f.getData(), strlen(f.getData()));
                file.close();
            }
            m = Message("ACK");
            sendMessage(std::move(m));
        };
    //});
    //download.detach();
}


void Client::inviaFile(filesystem::path p, FileStatus status) /*throw (std::runtime_error, filesystem::filesystem_error)*/{
    /*while(true){
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
    */
    //controllo checksum ed eventuale sincro
    //if(checksumClient != checksumServer){
        sincronizzaFile(p, status);
    //}
}

void Client::sincronizzaFile(filesystem::path path_to_watch, FileStatus status) /*throw(filesystem::filesystem_error, std::runtime_error)*/{
    //std::thread synch([this, path_to_watch, status] (Socket sock) -> void {
        //std::lock_guard<std::mutex> lg(this->mu);

        //path p(path_to_watch);
        std::stringstream ss;
        while(true){
            std::cout << "READ PATH: "<< path_to_watch.c_str() << std::endl;
            if(is_directory(dir/path_to_watch)){
                sendMessage(Message("DIRECTORY"));
                Message m = awaitMessage(); //attendo un response da server
                //invio directory da sincronizzare
                FileWrapper f = FileWrapper(path_to_watch, strdup(""), status);

                Message m2 = Message(std::move(f));
                //m2.print();
                sendMessage(std::move(m2));

                std::cout << "DIRECTORY SEND " << m2.getFileWrapper().getPath() << std::endl;

                //ricevo response
                m = awaitMessage();

                //leggo messaggio da server
                if(m.getMessage().compare("ACK") == 0) break;
            }else if(is_regular_file(dir/path_to_watch)){
                //invio file da sincronizzare

                std::ifstream input(path_to_watch.string());
                if(input.bad()) {
                    Message m = Message("FS_ERR");
                    sendMessage(std::move(m));
                    throw std::runtime_error("Error reading file");
                }

                sendMessage(Message("FILE"));
                Message m = awaitMessage();

                std::stringstream sstr;
                int size=0;
                while(input >> sstr.rdbuf()){
                    size += sstr.str().length();
                }

                input.close();

                char* data = strdup(sstr.str().c_str());
                //std::cout << "contenuto FILE:" << data << std::endl;
                FileWrapper f = FileWrapper(path_to_watch, data, status);
                Message m2 = Message(std::move(f));

                sendMessage(std::move(m2));

                std::cout << "FILE SEND " << m2.getFileWrapper().getPath() << std::endl;

                //ricevo response
                m = awaitMessage();

                //leggo ACK
                if(m.getMessage().compare("ACK") == 0) break;
            }else if(!exists(path_to_watch)){ //file cancellato
                Message m = Message("FILE");
                sendMessage(std::move(m));
                FileWrapper f = FileWrapper(path_to_watch, strdup(""), status);

                Message m2 = Message(std::move(f));
                //m2.print();
                sendMessage(std::move(m2));

                std::cout << "FILE DELETED " << m2.getFileWrapper().getPath() << std::endl;
                break;
            }else{
                Message m = Message("ERR");
                sendMessage(std::move(m));
                throw std::runtime_error("File not supported");
            }
        }

        //ho finito di inviare file

   // }, std::move(sock));
    //synch.detach();
}

boost::filesystem::path Client::strip_root(const boost::filesystem::path& p) {
    const boost::filesystem::path& parent_path = p.parent_path();
    if (parent_path.empty() || parent_path.string() == "/" || parent_path.string() == ".")
        return boost::filesystem::path();
    else
        return strip_root(parent_path) / p.filename();
}
