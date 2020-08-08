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
    std::cout <<"CONTENT DIR: " << std::endl;

    for(filesystem::directory_entry& d : filesystem::recursive_directory_iterator(this->folderPath)){
        v.push_back(removeFolderPath((d.path().string())));
         //std::cout << removeFolderPath((d.path().string())) <<std::endl;
    }
    return v;
}

// Create or replace file using specified path and data in buf
bool Folder::writeFile(const filesystem::path &path, char *buf, size_t size) {
    filesystem::path filePath;

    filePath = this->folderPath/path;

    std::cout <<"WRITE FILE PATH: " << filePath << std::endl;

    try {
        // If file doesn't exist create a folder for it (recursively if needed)
        if(!filesystem::exists(filePath.parent_path()))
            filesystem::create_directories(this->folderPath);

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
    filesystem::path filePath = this->folderPath/path;

    std::cout << "WRITE DIRECTORY PATH: " << filePath << std::endl;
    try {
        filesystem::create_directories(filePath);
    }
    catch (filesystem::filesystem_error& e) { // File opening might cause a filesystem_error
        std::cout << e.what();
        return false;
    }

    return true;
}

bool Folder::readFile(const filesystem::path &path, char *buf, size_t size) {
    filesystem::path filePath = this->folderPath/path;

    std::cout <<"READ FILE PATH: " << filePath << std::endl;

    if(!filesystem::exists(filePath))
        return false;

    try {
        size_t fileSize = filesystem::file_size(filePath);
        if (size > fileSize) size = fileSize;
        //std::cout << filePath << " " << size <<std::endl;
        filesystem::ifstream file;
        file.open(filePath, ios::in | ios::binary);
        file.read(buf, size-1);
        
        file.close();
        std::cout <<"BUFFER: " << buf <<std::endl;
        
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

    std::cout <<"GET SIZE PATH: " << filePath <<  " SIZE: " <<  filesystem::file_size(filePath) << std::endl;
    return filesystem::file_size(filePath);
}

// Delete file at the specified path
bool Folder::deleteFile(filesystem::path path) {
    filesystem::path filePath = this->folderPath/path;
    int removed = filesystem::remove_all(filePath);
    return removed > 0; //might change it to something better
}

// Calculate checksum and return/save it;
uint32_t Folder::getChecksum() {
    Checksum checksum = Checksum();
    for(filesystem::path path: this->getContent()){
        filesystem::path path_complete = this->getPath()/path;
        if(is_directory(path_complete))
            checksum.add(path.string());
        else{
            size_t size = getFileSize(path);
            if(size == -1) throw std::runtime_error("Error size file");
            char* buf = new char[size];
            if(!readFile(path, buf, size)) throw std::runtime_error("Error size file");
            std::string ss(buf);
            checksum.add(path.string() +  " " + ss); //calcolo checksum in base al percorso relativo del file + suo contenuto
        }
    }

    return checksum.getChecksum();
}


void Folder::wipeFolder() {
    std::cout << "Wiping folder content" << std::endl;
    filesystem::remove_all(this->folderPath);
    filesystem::create_directory(this->folderPath);
}

//toglie la directory user dal path
boost::filesystem::path Folder::strip_root(const boost::filesystem::path& p) {
    const boost::filesystem::path& parent_path = p.parent_path();
    if (parent_path.empty() || parent_path.string() == "/" || parent_path.string() == ".")
        return boost::filesystem::path();
    else
        return strip_root(parent_path) / p.filename();
}

std::string Folder::removeFolderPath(std::string path){
    if(path.find(this->getPath().string()) != std::string::npos){
        int lengthPath = folderPath.string().length();
        std::string directory_monitor = path.substr(lengthPath+1, path.length() - (lengthPath));
        return directory_monitor; //return prova/a/....
    }
    return path;
} 