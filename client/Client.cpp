#include <iostream>
#include <thread>
#include <mutex>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string>
#include <list>
#include <map>
#include <netinet/in.h>
#include <functional>
#include <sys/stat.h>
#include <shared_mutex>
#include <sys/sendfile.h>
#include <fcntl.h>
#include <string.h>
#include <sstream>
#include <optional>
#include <boost/filesystem.hpp>
#include "Client.h"

using namespace boost::filesystem;
using client = std::shared_ptr<Client>;


std::mutex mout;
void log(std::string msg){
    std::lock_guard<std::mutex> lg(mout);
    std::cout<<msg<<std::endl;
}

std::string Client::readline(){ //username o directory
     std::string buffer;
     char c;
     while(recv(sock, &c, 1, 0) > 0){
        if(c=='\n')
          return buffer;
        else if(c |= '\r') buffer += c;
        }
     return std::string();
}

void Client::close(){
    this->status = exit;
     //this->server->disattivaClient(this->sock);
     shutdown(sock, SHUT_RDWR);
}

void Client::handleConnection(){
    std::thread controllaDirectory([this](){
        log("USER:");
        std::string user = this->readline();
        while(user.empty()){ 
            log("Errore: inserire un username valido");
            log("USER:");
            user = this->readline();
        }

        if(/*server->findClient(user)*/ nullptr == nullptr)
        {
            log("DIRECTORY:");
            std::string directory = this->readline();
            this->status = active;
            path dir(directory);
            sock = socket(AF_INET, SOCK_STREAM, 0);
            if(sock == -1)
            {
               log("socket creation failed");
               return;
            }
            //saddr.sin_family = AF_INET;
            //saddr.sin_port = ntohs(server->port);

            if(exists(dir) && is_directory(dir)){
                if(/*server->findDirectory(directory, user)*/ true){
                    //TO DO eseguo checksum su le due directory
                }else{
                    path dir(directory);
                    for (auto& p : recursive_directory_iterator(dir)) {
                         if(is_directory(p)){//invio il nome della cartella al server
                           send(sock, p.path().relative_path().c_str(), strlen(p.path().relative_path().c_str())+1, 0);
                           recv(sock, &status, sizeof(int), 0);
                           if(status)
                                log("OK");
                           else
                                throw std::runtime_error("error");
                         }
                         else if(is_regular_file(p)) //invio il file e il suo contenuto
                         {
                           struct stat obj;
                           int size, filehandle;
                           filehandle = open(p.path().relative_path().c_str(), O_RDONLY);
                           if(filehandle==-1){
                                 throw std::runtime_error("eimpossibile aprire file");
                           }else{
                                //invio nome file
                                send(sock, p.path().relative_path().c_str(), strlen(p.path().c_str())+1, 0);
                                //calcolo la sua dimensione e la invio al server
                                stat(p.path().c_str(), &obj); 
                                size = obj.st_size;
                                send(sock, &size, sizeof(int), 0);
                                //invio file
                                sendfile(sock, filehandle, NULL, size);
                                //ricevo un ack da server se è andato tutto a buon fine
                                recv(sock, &status, sizeof(int), 0);
                                if(status)
                                    log("OK");
                                else
                                   throw std::runtime_error("error");
                           }
                         }
                    }
                }
            }
            else if(!exists(dir)){
                path p(directory);
                if(!create_directory(p))
                   throw std::runtime_error("impossibile creare directory");
                else
                {
                    send(sock, p.relative_path().c_str(), strlen(p.relative_path().c_str())+1, 0);
                    recv(sock, &status, sizeof(int), 0);
                    if(status)
                        log("OK");
                    else
                        throw std::runtime_error("error");
                }
            }

            //mi metto in ascolto e attendo una modifica
            FileWatcher fw{directory, std::chrono::milliseconds(1000)};
            fw.start([this](std::string path_to_watch, FileStatus status) -> void {
                switch(status) {
                    case FileStatus::created:
                    std::cout << "Created: " << path_to_watch << '\n';
                    send(sock, std::string("created").c_str(), strlen( std::string("created").c_str())+1, 0);
                    //dico al server che è stato creato un file e lo invio al server
                    int ret = inviaFile(path_to_watch);
                    if(ret < 0)
                        throw std::runtime_error("errore invio file");
                    break;
                    case FileStatus::modified:
                    //dico al server che è stato modificato un file e lo invio al server
                    std::cout << "Modified: " << path_to_watch << '\n';
                    send(sock, std::string("modified").c_str(), strlen( std::string("modified").c_str())+1, 0);
                    int ret = inviaFile(path_to_watch);
                    if(ret < 0)
                        throw std::runtime_error("errore invio file");
                    
                    break;
                    case FileStatus::erased:
                    //dico al server che è stato modificato un file e lo invio al server
                    send(sock, std::string("erased").c_str(), strlen( std::string("erased").c_str())+1, 0);
                    std::cout << "Erased: " << path_to_watch << '\n';
                    int ret = inviaFile(path_to_watch);
                    if(ret < 0)
                        throw std::runtime_error("errore invio file");
                    
                    break;
                    default:
                    break;
                }
  	        });
        }
        else{
            //log("utente già esistente!");
            //close();
            //return;
        }  
    });
    controllaDirectory.detach();
}

int Client::inviaFile(std::string path_){
    path p(path_);
    if(is_directory(p)){//invio il nome della cartella al server
         send(sock, p.relative_path().c_str(), strlen(p.relative_path().c_str())+1, 0);
         recv(sock, &status, sizeof(int), 0);
         return status;                     
    }
    if(is_regular_file(p)) //invio il file e il suo contenuto
    {
        struct stat obj;
        int size, filehandle;
        filehandle = open(p.relative_path().c_str(), O_RDONLY);
        if(filehandle==-1){
                return -1;
        }else{
            //invio nome file
            send(sock, p.relative_path().c_str(), strlen(p.c_str())+1, 0);
            //calcolo la sua dimensione e la invio al server
            stat(p.c_str(), &obj); 
            size = obj.st_size;
            send(sock, &size, sizeof(int), 0);
            //invio file
            sendfile(sock, filehandle, NULL, size);
            //ricevo un ack da server se è andato tutto a buon fine
            recv(sock, &status, sizeof(int), 0);
            return status;
        }
    }
    return -2;
}

