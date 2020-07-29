#include "FileWatcher.h"

// Monitor "path_to_watch" for changes and in case of a change execute the user supplied "action" function
void FileWatcher::start(const std::function<void (std::string, FileStatus)> &action) {
 while(running) {
     // Wait for "delay" milliseconds
     std::this_thread::sleep_for(delay);

     auto it = paths.begin();
     while (it != this->paths.end()) {
        if (!exists(it->first)) {
             action(it->first, FileStatus::erased);                     
	    it = paths.erase(it);
         }
         else {
             it++;
         }
    }

     // Check if a file was created or modified
     for(auto &file : recursive_directory_iterator(path_to_watch)) {
         auto current_file_last_write_time = last_write_time(file);

         // File creation
         if(!contains(file.path().string())) {
             paths[file.path().string()] = current_file_last_write_time;
             action(file.path().string(), FileStatus::created);
         // File modification
         } else {
             if(paths[file.path().string()] != current_file_last_write_time) {
                 paths[file.path().string()] = current_file_last_write_time;
                 action(file.path().string(), FileStatus::modified);
             }
         }
     }
}
}


