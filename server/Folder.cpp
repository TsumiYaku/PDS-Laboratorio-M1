#include "Folder.h"
#include <ios>
#include <iostream>

using std::ios;

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
bool Folder::writeFile(const filesystem::path &path, char *buf, size_t size) {
    filesystem::path filePath = this->folderPath/path;

    try {
        // If file doesn't exist create a folder for it (recursively if needed)
        if(!filesystem::exists(filePath.parent_path()))
            filesystem::create_directories(filePath.parent_path());

        // Write buf data into the file (might need to change to binary)
        filesystem::ofstream file;
        file.open(filePath, ios::out | ios::trunc | ios::binary);
        file.write(buf, size);
        file.close();
    }
    catch (filesystem::filesystem_error& e) { // File opening might cause a filesystem_error
        std::cout << e.what();
        return false;
    }

    return true;
}

bool Folder::readFile(const filesystem::path &path, char *buf, size_t size) {
    filesystem::path filePath = this->folderPath/path;

    if(!filesystem::exists(filePath))
        return false;

    try {
        size_t fileSize = filesystem::file_size(filePath);
        if (size > fileSize) size = fileSize;

        filesystem::ifstream file;
        file.open(filePath, ios::in | ios::binary);
        file.read(buf, size);
    }
    catch (filesystem::filesystem_error& e) { // File opening might cause a filesystem_error
        std::cout << e.what();
        return false;
    }

    return true;
}

ssize_t Folder::getFileSize(const filesystem::path &path) {
    filesystem::path filePath = this->folderPath/path;

    if(!filesystem::exists(filePath))
        return -1;

    return filesystem::file_size(filePath);
}

// Delete file at the specified path
bool Folder::deleteFile(filesystem::path path) {
    int removed = filesystem::remove_all(this->folderPath/path);
    return removed > 0; //might change it to something better
}

// Calculate checksum and return/save it;
Checksum Folder::getChecksum() {
    Checksum checksum = Checksum();
    for(filesystem::path path: this->getContent())
        checksum.add(path.string());

    return checksum;
}



