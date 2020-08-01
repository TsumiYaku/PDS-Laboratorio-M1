#include "FileWrapper.h"

FileWrapper::FileWrapper(filesystem::path filePath, char *data, FileStatus status):
    filePath(filePath), data(data), status(status)
{
    len = sizeof(data);
}
