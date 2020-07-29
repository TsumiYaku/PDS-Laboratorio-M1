#include "ServerSocket.h"
#include <arpa/inet.h>
#include <stdexcept>

ServerSocket::ServerSocket(int port) {
    struct sockaddr_in sockaddrIn;
    sockaddrIn.sin_port = htons(port);
    sockaddrIn.sin_family = AF_INET;
    sockaddrIn.sin_addr.s_addr = htonl(INADDR_ANY);

    if(::bind(sockfd, reinterpret_cast<struct sockaddr*>(&sockaddrIn), sizeof(sockaddrIn))!=0)
        throw std::runtime_error("Error during port binding");
    if(::listen(sockfd, 8) != 0)
        throw std::runtime_error("Error during port binding");

}

Socket ServerSocket::accept(struct sockaddr_in *addr, unsigned int *len) {
    int fd = ::accept(sockfd, reinterpret_cast<struct sockaddr*>(addr), len);
    if (fd<0) throw std::runtime_error("Error during socket acceptance");
    return Socket(fd);
}
