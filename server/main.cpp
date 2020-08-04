#include <iostream>
#include <bitset>
#include <fstream>
#include "Folder.h"
#include <boost/asio.hpp>
#include <packets/FileWrapper.h>

using namespace boost;
using std::ios;

int main() {
    Folder fold = Folder("Test", "Test");

    char testBuf[] = {'T', 'e', 's', 't'};

    filesystem::path testPath("subTest/subTest2/test.txt");

    // File creation and checksum test
    fold.deleteFile(testPath);

    std::cout << "Checksum test" << std::endl;

    std::bitset<32> a(fold.getChecksum());
    std::cout << a << std::endl;

    fold.writeFile(testPath, testBuf, sizeof(testBuf));

    std::bitset<32> b(fold.getChecksum());
    std::cout << b << std::endl;

    // Read/write test
    fold.deleteFile(testPath);

    std::cout << "\nRead/Write test" << std::endl;
    std::cout << "\nTest buffer: "<< testBuf << std::endl;

    fold.writeFile(testPath, testBuf, sizeof(testBuf));

    ssize_t fileSize = fold.getFileSize(testPath);
    char* readBuf = new char[fileSize];
    fold.readFile(testPath, readBuf, fileSize);

    std::cout << "Buffer after read/write: " << readBuf << std::endl;

    fold.deleteFile(testPath);

    fold.writeFile(testPath, readBuf, sizeof(testBuf));

    fileSize = fold.getFileSize(testPath);
    delete[] readBuf;
    readBuf = new char[fileSize];
    fold.readFile(testPath, readBuf, fileSize);

    std::cout << "Buffer after read/write 2: " << readBuf << std::endl;

}
