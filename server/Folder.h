#ifndef SERVER_FOLDER_H
#define SERVER_FOLDER_H

#include <string>
#include <vector>
#include <boost/filesystem.hpp>

using namespace boost;

class Folder {
    std::string owner;
    std::string name;
    filesystem::path folderPath;
    std::string checksum;
    

public:
    Folder(const std::string &owner, const std::string &path);

    std::vector<filesystem::path> getContent();
    bool writeFile(const filesystem::path &path, char* buf, int size);
    bool deleteFile(filesystem::path);
    std::string getChecksum();

};


#endif //SERVER_FOLDER_H
