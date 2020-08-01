#pragma once

#include <boost/filesystem.hpp>

using namespace boost;
enum class FileStatus {created, modified, erased};

class FileWrapper {
    filesystem::path filePath;
    char* data;
    size_t len;
    FileStatus status;

public:
    FileWrapper(filesystem::path filePath, char* data, FileStatus status);
};


