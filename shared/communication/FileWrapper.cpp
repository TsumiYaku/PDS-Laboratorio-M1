#include "FileWrapper.h"
#include <iostream>
#include <string.h>

using namespace boost;
using namespace serialization;


FileWrapper::FileWrapper()
{

}

FileWrapper::FileWrapper(filesystem::path filePath, FileStatus status, int len):
    filePath(filePath), status(status), len(len)
{

}

FileWrapper::FileWrapper(FileWrapper &&other) {
    filePath = other.filePath;
    len = other.len;
    status = other.status;
}

FileWrapper &FileWrapper::operator=(FileWrapper &&other) noexcept {
    if (this != &other) {
        filePath = other.filePath;
        len = other.len;
        status = other.status;
    }

    return *this;
}

filesystem::path FileWrapper::getPath(){
    return filePath;
}

void FileWrapper::setSize(int size){
    len = size;
}

int FileWrapper::getSize(){
    return len;
}

void FileWrapper::print(){
    std::cout <<"PATH: "<< filePath.relative_path().string() << std::endl;
    std::cout <<"SIZE:" << len << std::endl;
}

FileStatus FileWrapper::getStatus() {
    return status;
}





