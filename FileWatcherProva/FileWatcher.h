#include <boost/filesystem.hpp>
#include <functional>
#include <chrono>
#include <thread>
#include <unordered_map>
#include <string>
using namespace boost::filesystem;

enum class FileStatus {created, modified, erased};

class FileWatcher {
 public:
     std::string path_to_watch;
     std::chrono::duration<int, std::milli> delay;
 
     
    FileWatcher(std::string path_to_watch, std::chrono::duration<int, std::milli> delay) : path_to_watch{path_to_watch}, delay{delay} {
         path dir(path_to_watch);
         for(auto &file : recursive_directory_iterator(dir)) {
             paths[file.path().string()] = last_write_time(file);
         }
     }

     void start(const std::function<void (std::string, FileStatus)> &action);

     ~FileWatcher(){}

 private:
     std::unordered_map<std::string, time_t> paths; //percorsi dei file
     bool running = true;
     
     bool contains(const std::string &key) {
         auto el = paths.find(key);
         return el != paths.end();
     }
};

