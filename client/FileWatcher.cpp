#include <FileWatcher.h>

std::mutex mout;
void log(std::string msg){
    std::lock_guard<std::mutex> lg(mout);
    std::cout<<msg<<std::endl;
}

// Monitor "path_to_watch" for changes and in case of a change execute the user supplied "action" function
void FileWatcher::start(const std::function<void (std::string, FileStatus)> &action) {
 while(this->running_) {
     // Wait for "delay" milliseconds
     std::this_thread::sleep_for(delay);

     auto it = this->paths_.begin();
     while (it != paths_.end()) {
        if (!std::filesystem::exists(it->first)) {
             action(it->first, FileStatus::erased);                     
	    it = paths_.erase(it);
         }
         else {
             it++;
         }
    }

     // Check if a file was created or modified
     for(auto &file : std::filesystem::recursive_directory_iterator(path_to_watch)) {
         auto current_file_last_write_time = std::filesystem::last_write_time(file);

         // File creation
         if(!contains(file.path().string())) {
             paths_[file.path().string()] = current_file_last_write_time;
             action(file.path().string(), FileStatus::created);
         // File modification
         } else {
             if(paths_[file.path().string()] != current_file_last_write_time) {
                 paths_[file.path().string()] = current_file_last_write_time;
                 action(file.path().string(), FileStatus::modified);
             }
         }
     }
 }
}


