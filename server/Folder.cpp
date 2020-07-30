#include "Folder.h"
#include <ios>
#include <iostream>

Folder::Folder(const std::string &owner, const std::string &path) : owner(owner), folderPath(filesystem::path(path)) {
    // Create directory if not present
    filesystem::create_directory(this->folderPath);
}

// Retrieve folder content
std::vector<filesystem::path> Folder::getContent() {
    std::vector<filesystem::path> v;

    // Recursive research inside the folder
    for(filesystem::directory_entry& d : filesystem::recursive_directory_iterator(this->folderPath))
        v.push_back(d.path());

    return v;
}

// Create or replace file using specified path and data in buf
bool Folder::writeFile(const filesystem::path &path, char *buf, int size) {
    filesystem::path filePath = this->folderPath/path;

    try {
        // If file doesn't exist create a folder for it (recursively if needed)
        if(!filesystem::exists(filePath.parent_path()))
            filesystem::create_directories(filePath.parent_path());

        // Write buf data into the file (might need to change to binary)
        filesystem::ofstream file;
        file.open(filePath, std::ios::out | std::ios::trunc);
        file << buf;
        file.close();
    }
    catch (filesystem::filesystem_error& e) { // File opening might cause a filesystem_error
        std::cout << e.what();
        return false;
    }

    return true;
}

// Delete file at the specified path
bool Folder::deleteFile(filesystem::path path) {
    int removed = filesystem::remove_all(this->folderPath/path);
    return removed > 0; //might change it to something better
}

// Calculate checksum and return/save it; TODO: implement
std::string Folder::getChecksum() {
    return std::string();
}

