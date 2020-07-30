#pragma once

/* Class for managing the folder belonging to a user */

#include <string>
#include <vector>
#include <boost/filesystem.hpp>

using namespace boost;

class Folder {
    std::string owner; // Owner of the folder
    std::string name; // Name of the folder; TODO: check if actually needed
    filesystem::path folderPath; // Path in the filesystem where the folder is currently present
    std::string checksum; // Checksum indicating folder content;

public:
    Folder(const std::string &owner, const std::string &path);

    std::vector<filesystem::path> getContent(); // Return content of the folder
    bool writeFile(const filesystem::path &path, char* buf, int size); // Writes a file in the specified path
    bool deleteFile(filesystem::path); // Deletes a file in the specified path
    std::string getChecksum(); // Calculate and return checksum;

};
