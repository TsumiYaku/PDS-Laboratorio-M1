#include "FileWrapper.h"
#include <iostream>
#include <string.h>

using namespace boost;
using namespace serialization;

FileWrapper::FileWrapper()
{
    data = nullptr;
}

FileWrapper::FileWrapper(filesystem::path filePath, char *data, FileStatus status):
    filePath(filePath), data(data), status(status)
{
    len = strlen(data);
}

void FileWrapper::print(){
    std::cout <<"PATH: "<< filePath.relative_path().string() << std::endl;
    std::cout <<"DATA:" << data << std::endl;
    std::cout <<"SIZE:" << len << std::endl;
}

