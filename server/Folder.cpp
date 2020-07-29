#include "Folder.h"
#include <ios>
#include <iostream>

std::vector<filesystem::path> Folder::getContent() {
    std::vector<filesystem::path> v;
    for(filesystem::directory_entry& d : filesystem::recursive_directory_iterator(this->folderPath))
        v.push_back(d.path());
    return v;
}

Folder::Folder(const std::string &owner, const std::string &path) : owner(owner), folderPath(filesystem::path(path)) {
    filesystem::create_directory(this->folderPath);
}

// Create or replace file using specified path and data in buf
bool Folder::writeFile(const filesystem::path &path, char *buf, int size) {
    filesystem::path filePath = this->folderPath/path;

    try {
        if(!filesystem::exists(filePath.parent_path()))
            filesystem::create_directory(filePath.parent_path());

        std::cout << filePath << std::endl;

        filesystem::ofstream file;
        file.open(filePath, std::ios::out | std::ios::trunc);
        file << buf;
        file.close();
    }
    catch (filesystem::filesystem_error& e) {
        e.what();
        return false;
    }

    return true;
}

// delete the file at specified path
bool Folder::deleteFile(filesystem::path) {
    return false;
}

// TODO: calculate checksum and return/save it
std::string Folder::getChecksum() {
    return std::string();
}

