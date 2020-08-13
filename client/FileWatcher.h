#pragma once

#include <boost/filesystem.hpp>
#include <functional>
#include <chrono>
#include <thread>
#include <unordered_map>
#include <string>
#include <packets/FileStatus.h>

using namespace boost::filesystem;

class FileWatcher {
 private:
     std::unordered_map<std::string, time_t> paths; //percorsi dei file
     bool running = true;
     
     bool contains(const std::string &key) {
         auto el = paths.find(key);
         return el != paths.end();
     }
     std::string path_to_watch;
     std::chrono::duration<int, std::milli> delay;
     std::mutex mu;
     std::mutex mu2;
     std::mutex mu3;
     bool locked;
     bool first_syncrinize;
     bool freezed = false;
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
     //non esegue nessuna azione
     void lock();
     void unlock();
     bool getLocked();
     //mette le varie azioni in una coda e verranno processate dopo la prima sincronizzazione
     void first_syncro();
     void not_first_syncro();
     bool getFirstSyncro();

      void freeze();
      void restart();
     
     
     ~FileWatcher(){}

};

