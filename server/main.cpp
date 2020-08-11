#include "Server.h"

int main(int argc, char* argv[]) {
    if(argc != 2) {
        std::cout<<"USAGE ./server PORT"<<std::endl;
        exit(-1);
    }

    Server s(atoi(argv[1]));
    s.run();

    return 0;
}
