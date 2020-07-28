#include "./FileWatcher.h"
#include <iostream>

int main(){
    FileWatcher fw{"./prova", std::chrono::milliseconds(1000)};
    fw.start([] (std::string path_to_watch, FileStatus status) -> void {
 
         switch(status) {
             case FileStatus::created:
                 std::cout << "File created: " << path_to_watch << '\n';
                 //
                 break;
             case FileStatus::modified:
                 std::cout << "File modified: " << path_to_watch << '\n';
                 //
                 break;
             case FileStatus::erased:
                 std::cout << "File erased: " << path_to_watch << '\n';
                 //
                 break;
             default:
                 break;
         }
    });
}
