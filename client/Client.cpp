#include <iostream>
#include <thread>
#include <mutex>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string>
#include <list>
#include <map>
#include <functional>
#include <shared_mutex>
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
            if(exists(dir) && is_directory(dir)){
                if(/*server->findDirectory(directory, user)*/ true){
                    //TO DO: ho trovato la cartella sul server per quel user e verifico se combacia 
                    //con quella del client
                }else{
                    //TO DO: invierò il contenuto dei vari file e cartelle al server
                    //per potermi sincronizzare e creare la cartella lato server
                }
            }
            else if(!exists(dir)){
                //TO DO: errore, creo cartella
            }

            //mi metto in ascolto e attendo una modifica
            FileWatcher fw{directory, std::chrono::milliseconds(1000)};
            fw.start([](std::string path_to_watch, FileStatus status) -> void {
                switch(status) {
                    case FileStatus::created:
                    std::cout << "File created: " << path_to_watch << '\n';
                    //TO DO interazione con server
                    break;
                    case FileStatus::modified:
                    std::cout << "File modified: " << path_to_watch << '\n';
                    //TO DO interazione con server
                    break;
                    case FileStatus::erased:
                    std::cout << "File erased: " << path_to_watch << '\n';
                    //TO DO interazione con server
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

