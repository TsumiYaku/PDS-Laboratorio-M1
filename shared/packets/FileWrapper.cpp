#include "FileWrapper.h"
#include <iostream>


using namespace boost;
using namespace serialization;

FileWrapper::FileWrapper()
{
    
}
FileWrapper::FileWrapper(filesystem::path filePath, char *data, FileStatus status):
    filePath(filePath), data(data), status(status)
{
    len = sizeof(data);
}

void FileWrapper::print(){
    std::cout <<"PATH: "<< filePath.relative_path().string() << std::endl;
    std::cout <<"DATA:" << data << std::endl;
    std::cout <<"SIZE:" << len << std::endl;
}

