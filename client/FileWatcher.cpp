#include "FileWatcher.h"
#include <iostream>

// Monitor "path_to_watch" for changes and in case of a change execute the user supplied "action" function
void FileWatcher::start(const std::function<void (std::string, FileStatus, bool, bool)> &action) {
 while(running) {
     try{
        // Wait for "delay" milliseconds
        std::this_thread::sleep_for(delay);
        //if(!freezed){
            auto it = paths.begin();
            while (it != this->paths.end()) {
                if (!exists(it->first)) {
                    action(it->first, FileStatus::erased, getLocked(), getFirstSyncro()); 
                    it = paths.erase(it);
                }
                else {
                    it++;
                }
            }
            
            // Check if a file was created or modified
            for(auto &file : recursive_directory_iterator(path_to_watch)) {
                if(!freezed){
                    time_t current_file_last_write_time = last_write_time(file);
                    //std::cout << current_file_last_write_time << " " << paths[file.path().string()] <<std::endl;
                    // File creation
                    if(!contains(file.path().string())) {
                        paths[file.path().string()] = current_file_last_write_time;
                        std::cout <<"create:" << file.path().string() << " " << current_file_last_write_time << std::endl;
                        action(file.path().string(), FileStatus::created, getLocked(), getFirstSyncro());
                    // File modification
                    } else {
                        std::cout <<"modify:" << file.path().string() << " " << current_file_last_write_time << std::endl;
                        if(paths[file.path().string()] != current_file_last_write_time) {
                            paths[file.path().string()] = current_file_last_write_time;
                            action(file.path().string(), FileStatus::modified, getLocked(), getFirstSyncro());
                        }else
                        {
                            action(file.path().string(), FileStatus::nothing, getLocked(), getFirstSyncro());
                        }
                    }

                }
                else{
                    action("", FileStatus::nothing, getLocked(), getFirstSyncro());
                }
            }
        //}else{
            //action("", FileStatus::nothing, getLocked(), getFirstSyncro());
        //}
     }catch(...){continue;};
   }
}

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


void FileWatcher::freeze(){
    std::lock_guard<std::mutex> lg(mu3);
    freezed = true;
}

void FileWatcher::restart(){
    std::lock_guard<std::mutex> lg(mu3);
    freezed = false;
}
