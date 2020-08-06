#include "Folder.h"
#include <ios>
#include <iostream>

using std::ios;

boost::filesystem::path Folder::strip_root(const boost::filesystem::path& p) {
    const boost::filesystem::path& parent_path = p.parent_path();
    if (parent_path.empty() || parent_path.string() == "/")
        return boost::filesystem::path();
    else
        return strip_root(parent_path) / p.filename();
}
///___________________________________

Folder::Folder(const std::string &owner, const std::string &path) : owner(owner), folderPath(filesystem::path(path)) {
    // Create directory if not present
    filesystem::create_directory(this->folderPath);
}

// Retrieve folder content
std::vector<filesystem::path> Folder::getContent() {
    std::vector<filesystem::path> v;
    
    // Recursive research inside the folder
    for(filesystem::directory_entry& d : filesystem::recursive_directory_iterator(this->folderPath))
        v.push_back(strip_root(d.path()));

    return v;
}

// Create or replace file using specified path and data in buf
bool Folder::writeFile(const filesystem::path &path, char *buf, size_t size) {
    filesystem::path filePath;
    //std::cout << path.string().substr(0,2) <<std::endl;
    if(path.string().substr(0,2) == "./"){
         filePath = this->folderPath/path.string().substr(2, path.string().length() - 2);
    }
    else
        filePath = this->folderPath/path;

    std::cout <<"WRITE FILE PATH: " << filePath << std::endl;

    try {
        // If file doesn't exist create a folder for it (recursively if needed)
        if(!filesystem::exists(filePath.parent_path()))
        {  
            filesystem::create_directories(this->folderPath);

        }

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

filesystem::path Folder::getPath(){
    return this->folderPath;
}

bool Folder::writeDirectory(const filesystem::path &path) {
    filesystem::path filePath;
    //std::cout << path.string().substr(0,2) <<std::endl;
    if(path.string().substr(0,2) == "./"){
         filePath = this->folderPath/path.string().substr(2, path.string().length() - 2);
    }
    else
        filePath = this->folderPath/path;

    std::cout << "WRITE DIRECTORY PATH: " << filePath << std::endl;
    try {
        // If file doesn't exist create a folder for it (recursively if needed)
        if(!filesystem::exists(filePath.parent_path()))
        {  
            //std::cout << filePath.string() << std::endl;
            filesystem::create_directories(this->folderPath);
        }
        filesystem::create_directories(filePath);
    
    }
    catch (filesystem::filesystem_error& e) { // File opening might cause a filesystem_error
        std::cout << e.what();
        return false;
    }

    return true;
}

bool Folder::readFile(const filesystem::path &path, char *buf, size_t size) {

    std::cout <<"READ FILE PATH: " << path << std::endl;

    filesystem::path filePath = this->folderPath/path;

    if(!filesystem::exists(path))
        return false;

    try {
        size_t fileSize = filesystem::file_size(filePath);
        if (size > fileSize) size = fileSize;
        std::cout << path << " " << size <<std::endl;
        filesystem::ifstream file;
        file.open(path, ios::in | ios::binary);
        file.read(buf, size);
        std::cout << buf <<std::endl;
    }
    catch (filesystem::filesystem_error& e) { // File opening might cause a filesystem_error
        std::cout << e.what();
        return false;
    }

    return true;
}

ssize_t Folder::getFileSize(const filesystem::path &path) {

    std::cout <<"GET SIZE PATH: " << path << std::endl;

    if(!filesystem::exists(path))
        return -1;

    return filesystem::file_size(path);
}

// Delete file at the specified path
bool Folder::deleteFile(filesystem::path path) {
    int removed = filesystem::remove_all(this->folderPath/path);
    return removed > 0; //might change it to something better
}

// Calculate checksum and return/save it;
uint32_t Folder::getChecksum() {
    Checksum checksum = Checksum();
    for(filesystem::path path: this->getContent())
        checksum.add(path.string());

    return checksum.getChecksum();
}

//toglie la directory user dal path
boost::filesystem::path Folder::strip_root(const boost::filesystem::path& p) {
    const boost::filesystem::path& parent_path = p.parent_path();
    if (parent_path.empty() || parent_path.string() == "/")
        return boost::filesystem::path();
    else
        return strip_root(parent_path) / p.filename();
}
