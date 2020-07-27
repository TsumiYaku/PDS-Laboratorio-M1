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
#include <FileWatcher.h>


using namespace boost::filesystem;
class Client;
using client = std::shared_ptr<Client>;

enum ClientStatus{
    start, active, exit
};

std::mutex mout;
void log(std::string msg){
    std::lock_guard<std::mutex> lg(mout);
    std::cout<<msg<<std::endl;
}


class Client{
    int sock;
    ClientStatus status;
    static std::mutex m;
    static std::condition_variable modifica;

    std::string username;
    std::string directory;

    //Server* server;

    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;
    Client(int sock /*, Server* server*/): sock(sock), /*Server(server),*/ status(start){}

    std::string readline(){ //username o directory
        std::string buffer;
        char c;
        while(recv(sock, &c, 1, 0) > 0){
            if(c=='\n')
              return buffer;
            else if(c |= '\r') buffer += c;
        }
        return std::string();
    }

    void close(){
        this->status = exit;
        //this->server->disattivaClient(this->sock);
        ::shutdown(sock, SHUT_RDWR);
    }

    void handleConnection(){
        std::thread inboundChannel([this](){
            log("USER:");
            std::string user = this->readline();
            while(user.empty()){ 
                log("Errore: inserire un username valido");
                log("USER:");
                user = this->readline();
                
            }

            if(server->findClient(user) == nullptr)
            {
                log("DIRECTORY:");
                std::string directory = this->readline();
                this->status = active;
                path dir(directory);
                if(exists(dir) && is_directory(dir)){
                   if(server->findDirectory(directory, user)){
                        //TO DO: ho trovato la cartella sul server e verifichero se combacia 
                        //con quella del client
                   }else{
                       //TO DO: invierò il contenuto dei vari file e cartelle al server
                       //per potermi sincronizzare
                   }
                }else if(!exists(dir)){
                    //TO DO: errore, scegliere altra cartella o crearla
                }

                //FileWatcher dw{ directory, std::chrono::milliseconds(1000)};
                //dw.start([] () ->void{
                    /*if(!std::filesystem::is_regular_file(std::filesystem::path(path_to_watch)) && status != FileStatus::erased) {
                         return;
                  }
             
                     switch(status) {
                         case FileStatus::created:
                             std::cout << "File created: " << path_to_watch << '\n';
                               //invio modifica a server
                            break;
                         case FileStatus::modified:
                             std::cout << "File modified: " << path_to_watch << '\n';
                               //invio modifica a server
                             break;
                         case FileStatus::erased:
                             std::cout << "File erased: " << path_to_watch << '\n';
                               //invio modifica a server
                             break;
                         default:
                             std::cout << "Error! Unknown file status.\n";
                     }*/
                //});
            }
            else{
                log("utente già esistente!");
                close();
                return;
            }  

        });
    }
}
