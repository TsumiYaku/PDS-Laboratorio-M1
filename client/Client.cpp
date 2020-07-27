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
#pragma once

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

enum class FileStatus {created, modified, erased};
class FileWatcher {
 6 public:
       std::string path_to_watch;
 8     // Time interval at which we check the base folder for changes
 9     std::chrono::duration<int, std::milli> delay;
10 
11     // Keep a record of files from the base directory and their last modification time
12     FileWatcher(std::string path_to_watch, std::chrono::duration<int, std::milli> delay) : path_to_watch{path_to_watch}, delay{delay} {
13         for(auto &file : std::filesystem::recursive_directory_iterator(path_to_watch)) {
14             paths_[file.path().string()] = std::filesystem::last_write_time(file);
15         }
16     }

 9     // Monitor "path_to_watch" for changes and in case of a change execute the user supplied "action" function
10     void start(const std::function<void (std::string, FileStatus)> &action) {
11         while(running_) {
12             // Wait for "delay" milliseconds
13             std::this_thread::sleep_for(delay);
14 
15             auto it = paths_.begin();
16             while (it != paths_.end()) {
17                 if (!std::filesystem::exists(it->first)) {
18                     action(it->first, FileStatus::erased);
19                     it = paths_.erase(it);
20                 }
21                 else {
22                     it++;
23                 }
24             }
25 
26             // Check if a file was created or modified
27             for(auto &file : std::filesystem::recursive_directory_iterator(path_to_watch)) {
28                 auto current_file_last_write_time = std::filesystem::last_write_time(file);
29 
30                 // File creation
31                 if(!contains(file.path().string())) {
32                     paths_[file.path().string()] = current_file_last_write_time;
33                     action(file.path().string(), FileStatus::created);
34                 // File modification
35                 } else {
36                     if(paths_[file.path().string()] != current_file_last_write_time) {
37                         paths_[file.path().string()] = current_file_last_write_time;
38                         action(file.path().string(), FileStatus::modified);
39                     }
40                 }
41             }
42         }
43     }

44 private:
45     std::unordered_map<std::string, std::filesystem::file_time_type> paths_;
46     bool running_ = true;
47 
48     // Check if "paths_" contains a given key
49     // If your compiler supports C++20 use paths_.contains(key) instead of this function
50     bool contains(const std::string &key) {
51         auto el = paths_.find(key);
52         return el != paths_.end();
53     }
54 };


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

    void handleConection(){
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

                //FileWatcher dw{ ".", std::chrono::milliseconds(100)};
                //dw.start([] () ->void{
                    /*if(!std::filesystem::is_regular_file(std::filesystem::path(path_to_watch)) && status != FileStatus::erased) {
            14             return;
            15      }
            16 
            17         switch(status) {
            18             case FileStatus::created:
            19                 std::cout << "File created: " << path_to_watch << '\n';
                               //invio modifica a server
            20                 break;
            21             case FileStatus::modified:
            22                 std::cout << "File modified: " << path_to_watch << '\n';
                               //invio modifica a server
            23                 break;
            24             case FileStatus::erased:
            25                 std::cout << "File erased: " << path_to_watch << '\n';
                               //invio modifica a server
            26                 break;
            27             default:
            28                 std::cout << "Error! Unknown file status.\n";
            29         }*/
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
