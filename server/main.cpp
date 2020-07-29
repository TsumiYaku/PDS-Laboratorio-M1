#include <iostream>
#include "Folder.h"


int main() {
    if(!filesystem::exists("Test"))
        filesystem::create_directory("Test");
    Folder fold = Folder("Test", "Test");
    char buf[] = {'T', 'e', 's', 't'};

    fold.writeFile("test.txt", buf, 4);
}