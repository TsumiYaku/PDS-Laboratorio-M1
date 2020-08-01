#include <iostream>
#include <bitset>
#include "Folder.h"
#include <boost/asio.hpp>

using namespace boost;

int main() {
    if(!filesystem::exists("Test"))
        filesystem::create_directory("Test");
    Folder fold = Folder("Test", "Test");
    char buf[] = {'T', 'e', 's', 't'};

    fold.deleteFile("subTest/subTest2/test.txt");

    std::bitset<32> a(fold.getChecksum().getChecksum());
    std::cout << a << std::endl;

    fold.writeFile("subTest/subTest2/test.txt", buf, 4);

    std::bitset<32> b(fold.getChecksum().getChecksum());
    std::cout << b << std::endl;

    
}