#include "FileWatcher.h"
#include <iostream>

// Monitora "path_to_watch" . in caso di modifiche alla cartella allora richiama la funzione action
void FileWatcher::start(const std::function<void (std::string, FileStatus, bool)> &action) {
 while(running) {
     try{
        std::this_thread::sleep_for(delay);
        
        //controllo cancellazione
        auto it = paths.begin();
        while (it != this->paths.end()) {
            if (!exists(it->first)) {
                if(getLocked() == false)
                   action(it->first, FileStatus::erased, getFirstSyncro()); 
                it = paths.erase(it);
            }
            else {
                it++;
            }
        }
        
        for(auto &file : recursive_directory_iterator(path_to_watch)) {
            if(!freezed){
                time_t current_file_last_write_time = last_write_time(file);
                
                // File creation
                if(!contains(file.path().string())) {
                    paths[file.path().string()] = current_file_last_write_time;
                    if(getLocked() == false)
                     action(file.path().string(), FileStatus::created, getFirstSyncro());//path file, status, se il fileWather è in lock e se è in fase di prima syncro
                // File modification
                } else {
                    if(paths[file.path().string()] != current_file_last_write_time) {
                        paths[file.path().string()] = current_file_last_write_time;
                        if(getLocked() == false)
                            action(file.path().string(), FileStatus::modified, getFirstSyncro());
                    }else
                    {
                        if(getLocked() == false) 
                            action(file.path().string(), FileStatus::nothing, getFirstSyncro());
                    }
                }
            }
            else{
                action("", FileStatus::nothing, getFirstSyncro());
            }
        }
     }catch(...){continue;};
   }
}

//imposto che le modifiche del file watcher sono ignorate (no usato nell'effettivo)
void FileWatcher::lock(){
    std::lock_guard<std::mutex> lg(mu);
    locked = true;
}
bool FileWatcher::getLocked(){
    std::lock_guard<std::mutex> lg(mu);
   return locked;
}

void FileWatcher::unlock(){
    std::lock_guard<std::mutex> lg(mu);
    locked = false;
}


//il file watcher è in stato di prima sincronizzazione con il server
void FileWatcher::first_syncro(){
    std::lock_guard<std::mutex> lg(mu2);
    first_syncrinize = true;
}

bool FileWatcher::getFirstSyncro(){
    std::lock_guard<std::mutex> lg(mu2);
    return first_syncrinize;
}

void FileWatcher::not_first_syncro(){
    std::lock_guard<std::mutex> lg(mu2);
    first_syncrinize = false;
}


//il filewatcher è bloccato (non legge nessuna modifica (attende che venga sbloccato per leggerle))
void FileWatcher::freeze(){
    std::lock_guard<std::mutex> lg(mu3);
    freezed = true;
}

void FileWatcher::restart(){
    std::lock_guard<std::mutex> lg(mu3);
    freezed = false;
}
