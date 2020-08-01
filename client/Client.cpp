#include <iostream>
#include <thread>
#include <mutex>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string>
#include <netinet/in.h>
#include <functional>
#include <fcntl.h>
#include <string.h>
#include <sstream>
#include <boost/filesystem.hpp>
#include "Client.h"

using namespace boost::filesystem;

std::mutex mout;
void log(std::string msg){
    std::lock_guard<std::mutex> lg(mout);
    std::cout<<msg<<std::endl;
}

//calcola il checksum parteno dal path
Checksum getChecksum(path p) {
    Checksum checksum = Checksum();
    for(filesystem::path path: p)
        checksum.add(path.string());

    return checksum;
}

//CLIENT
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

void Client::monitoraCartella(){
    std::thread controllaDirectory([this](){

        //std::lock_guard<std::mutex> lg(m);
        /************************DA PORTARE AL MAIN FORSE******************/
        std::string user;
        do{
            log("USER:");
            user = readline();
            if(user.empty())
              log("Errore: inserire un username valido");
        }while(user.empty());
        //user ha inserito un nome non nullo ("")
        this->status = active;
        
        std::string directory;
        path dir;
        int stato;//per sapere se server ha letto corremìtamente file inviatogli
        do{
            log("DIRECTORY:");
            directory = readline();
            dir = path(directory);
            if(!is_directory(dir))
              log("Errore: inserire un path valido");
        }while(!is_directory(dir));
        /*****************************************************************/

        if(exists(dir)){ //la cartella esiste e quindi verifico se server ha gia cartella oppure no
            int cartellaTrovataServer = 0;
            sock.write(dir.relative_path().string().c_str(), strlen(dir.relative_path().c_str())+1, 0);//invio path a server
            sock.readInt(&cartellaTrovataServer, sizeof(cartellaTrovataServer), 0);//ricevo response da server
            if(!cartellaTrovataServer){//invio la sua copia al server
                inviaDirectory(dir);
            }else{
                //verifico se è gia sincornizzata con checksum
                Checksum check();
                int val = getChecksum(dir).getChecksum();
                sock.writeInt(&val, sizeof(val), 0);
                sock.readInt(&stato, sizeof(stato), 0);
                if(stato)
                    log("OK");//cartella gia sincronizzata
                else{
                    inviaDirectory(dir);//risincronizzo
                }   
            }
        }
        else{//creo cartella e la mando al server
                if(!create_directory(dir))
                   throw std::runtime_error("impossibile creare directory");
                else
                {
                    sock.write("new_directory", strlen("new_directory")+1, 0);
                    sock.write(dir.relative_path().c_str(), strlen(dir.relative_path().c_str())+1, 0);
                    sock.readInt(&stato, sizeof(stato), 0);
                    if(stato)
                        log("OK");
                    else
                        throw std::runtime_error("error");
                }
            }

            //mi metto in ascolto e attendo una modifica
            FileWatcher fw{directory, std::chrono::milliseconds(1000)};
            fw.start([this](std::string path_to_watch, FileStatus status) -> void {
                int ret;
                switch(status) {
                    case FileStatus::created:
                    std::cout << "Created: " << path_to_watch << '\n';
                    sock.write(std::string("created").c_str(), strlen( std::string("created").c_str())+1, 0);
                    //dico al server che è stato creato un file e lo invio al server
                    ret = inviaFile(path_to_watch);
                    if(ret < 0)//se la modifica non è andata a buon fine allora cerco di sincronizzarmi
                        sincronizza(path_to_watch);
                    break;
                    case FileStatus::modified:
                    //dico al server che è stato modificato un file e lo invio al server
                    std::cout << "Modified: " << path_to_watch << '\n';
                    sock.write(std::string("modified").c_str(), strlen( std::string("modified").c_str())+1, 0);
                    ret = inviaFile(path_to_watch);
                    if(ret < 0)//se la modifica non è andata a buon fine allora cerco di sincronizzarmi
                        sincronizza(path_to_watch);
                    
                    break;
                    case FileStatus::erased:
                    //dico al server che è stato modificato un file e lo invio al server
                    sock.write(std::string("erased").c_str(), strlen( std::string("erased").c_str())+1, 0);
                    std::cout << "Erased: " << path_to_watch << '\n';
                    ret = inviaFile(path_to_watch);
                    if(ret < 0)//se la modifica non è andata a buon fine allora cerco di sincronizzarmi
                        sincronizza(path_to_watch);
                    break;
                }
  	    }); 
    });

    controllaDirectory.detach();
}

int Client::inviaFile(std::string path_){
    path p(path_);
    int stato;
    if(is_directory(p)){//invio il nome della cartella al server
         sock.write(p.relative_path().c_str(), strlen(p.relative_path().c_str())+1, 0);
         sock.readInt(&stato, sizeof(int), 0);
         return stato;                     
    }
    if(is_regular_file(p)) //invio il file e il suo contenuto
    {
       sock.write(p.relative_path().c_str(), strlen(p.c_str())+1, 0);
        sock.writeFile(p.relative_path().string(), 0); 
        sock.readInt(&stato, sizeof(stato), 0);
        if(stato)
            log("OK");
        else
            throw std::runtime_error("error");
    }
    return -2;
}

void Client::inviaDirectory(path dir){
    int stato;
    for(directory_entry& p : recursive_directory_iterator(dir)) {
            if(is_directory(p)){
            sock.write(p.path().relative_path().string().c_str(), strlen(p.path().relative_path().c_str())+1, 0);
            sock.readInt(&stato, sizeof(stato), 0);
            if(stato)
                log("OK");
            else
                throw std::runtime_error("error");
        }
        else if(is_regular_file(p)) //invio il file e il suo contenuto
        {
            //invio nome file
            sock.write(p.path().relative_path().c_str(), strlen(p.path().c_str())+1, 0);
            sock.writeFile(p.path().relative_path().string(), 0); 
            sock.readInt(&stato, sizeof(stato), 0);
            if(stato)
                log("OK");
            else
                throw std::runtime_error("error");
        }
    }
}


void Client::sincronizza(std::string path_to_watch) {
    Checksum check();
    int ok = 0;
    int ret = 0;
    path p(path_to_watch);

    do{
        int val = getChecksum(p).getChecksum();

        sock.writeInt(&val, sizeof(val), 0);
        sock.readInt(&ok, sizeof(ok), 0);

        if(ok)
            log("OK");//cartella gia sincronizzata
        else{
            if(is_directory(p))
                inviaDirectory(p);//risincronizzo
            else if(is_regular_file(p))
                ret = inviaFile(path_to_watch);
        }
    }while(!ok && !ret);
}

