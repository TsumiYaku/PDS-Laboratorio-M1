#pragma once

#include <boost/filesystem.hpp>
#include <functional>
#include <chrono>
#include <thread>
#include <unordered_map>
#include <string>
#include <communication/FileWrapper.h>

using namespace boost::filesystem;

class FileWatcher {
 private:
     std::unordered_map<std::string, time_t> paths; //percorsi dei file
     bool running = true;//filewatcher è attivo o no
     
     bool contains(const std::string &key) {
         auto el = paths.find(key);
         return el != paths.end();
     }
     std::string path_to_watch;
     std::chrono::duration<int, std::milli> delay;
     std::mutex mu;
     std::mutex mu2;
     std::mutex mu3;
     bool locked;//il filesystem monitora ma ignora le modifiche (non le invierà al server)
     bool first_syncrinize;//cartella da monitorare è in fase di prima sincronizzazione con il server
     bool freezed = false;//filewatcher non guarda le modifiche e attende lo sblocco (se true)
public:

     FileWatcher(std::string path_to_watch, std::chrono::duration<int, std::milli> delay) : path_to_watch{path_to_watch}, delay{delay} {
         locked = false;
         first_syncrinize = true;
         freezed = false;
         path dir(path_to_watch);
         for(auto &file : recursive_directory_iterator(dir)) {
             paths[file.path().string()] = last_write_time(file);
         }
     }

     void start(const std::function<void (std::string, FileStatus, bool, bool)> &action);
     
     //LOCK
     //il filewatcher ignora le modifiche (richiama action ma non verranno inserite nella coda di richiesta del Client)
     void lock();
     void unlock();
     bool getLocked();
     
     //FILRST_SYNCRO
     //fase di prima sincronizzazione (devo attendere la sua fine per poter inviare tutte le richieste del client)
     void first_syncro();
     void not_first_syncro();
     bool getFirstSyncro();

    //FREEZE
    //il filewatcher è bloccato in attesa di sblocco. (quando si sbloccherà allora leggerà le nuove modifiche che si erà perso durante il freeze)
     void freeze();
     void restart();
     
     
     ~FileWatcher(){}

};

