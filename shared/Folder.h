#pragma once

/* Class for managing the folder belonging to a user */

#include <string>
#include <vector>
#include <boost/filesystem.hpp>
#include <Checksum.h>

using namespace boost;

enum FolderType {client, server};

class Folder {
    std::string owner; // Owner of the folder
    std::string name; // Name of the folder; TODO: check if actually needed
    filesystem::path folderPath; // Path in the filesystem where the folder is currently present

    

public:
    boost::filesystem::path strip_root(const boost::filesystem::path& p);
    Folder(const std::string &owner, const std::string &path);
    filesystem::path getPath();
    std::vector<filesystem::path> getContent(); // Return content of the folder
    bool writeFile(const filesystem::path &path, char* buf, size_t size); // Writes a file in the specified path
    bool writeDirectory(const filesystem::path &path); // Writes a directory in the specified path
    bool readFile(const filesystem::path &path, char* buf, size_t size);
    ssize_t getFileSize(const filesystem::path &path);
    bool deleteFile(filesystem::path); // Deletes a file in the specified path
    void wipeFolder(); // Completely wipes the content of the folder
    uint32_t getChecksum(); // Calculate and return checksum;
    std::string removeFolderPath(std::string p);
};
