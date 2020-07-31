#include <iostream>
#include "Folder.h"


int main() {
    if(!filesystem::exists("Test"))
        filesystem::create_directory("Test");
    Folder fold = Folder("Test", "Test");
    char buf[] = {'T', 'e', 's', 't'};

    fold.deleteFile("subTest/subTest2/test.txt");

    std::cout << (unsigned int)fold.getChecksum().getChecksum() << std::endl;

    fold.writeFile("subTest/subTest2/test.txt", buf, 4);

    std::cout << (unsigned int)fold.getChecksum().getChecksum() << std::endl;
}