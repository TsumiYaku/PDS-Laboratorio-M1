#include "Socket.h"
#include <iostream>
#include <unistd.h>
#include <stdexcept>

Socket::Socket(int sockfd): sockfd(sockfd) {}

Socket::Socket() {
    sockfd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) throw std::runtime_error("Error during socket creation");
    std::cout << "Socket " << sockfd << " initialized";
}

Socket::~Socket() {
    if (sockfd > 0) {
        std::cout << "Closing socket " << sockfd;
        close(sockfd);
    }
}

Socket::Socket(Socket &&other) {
    this->sockfd = other.sockfd;
}

Socket &Socket::operator=(Socket &&other) {
    if (sockfd > 0) close(sockfd);
    sockfd = other.sockfd;
    other.sockfd = 0;
    return *this;
}

ssize_t Socket::read(char *buf, size_t len, int options) {
    ssize_t res = recv(sockfd, buf, len, options);
    if (res < 0) throw std::runtime_error("Error during socket reading");
    return res;
}

ssize_t Socket::write(char *buf, size_t len, int options) {
    ssize_t res = send(sockfd, buf, len, options);
    if (res < 0) throw std::runtime_error("Error during data sending");
    return res;
}

void Socket::connect(struct sockaddr_in *addr, unsigned int len) {
    if(::connect(sockfd, reinterpret_cast<struct sockaddr*>(addr), len)!=0)
        throw std::runtime_error("Error during socket connection");
}


