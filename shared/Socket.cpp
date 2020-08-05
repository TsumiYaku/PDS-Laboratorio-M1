#include "Socket.h"
#include <iostream>
#include <unistd.h>
#include <stdexcept>
#include <string.h>
#include <netinet/in.h>
#include <errno.h>
#include<sys/sendfile.h>

Socket::Socket(int sockfd): sockfd(sockfd) {}

Socket::Socket() {
    sockfd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) throw std::runtime_error("Error during socket creation");
    std::cout << "Socket " << sockfd << " initialized" << std::endl;
}

Socket::~Socket() {
    if (sockfd > 0) {
        std::cout << "Closing socket " << sockfd <<std::endl;
        close(sockfd);
    }
}

Socket::Socket(Socket &&other) {
    if (sockfd > 0) close(sockfd);
    sockfd = other.sockfd;
    other.sockfd = 0;
    std::cout <<"SOCKET MOVE 2"<<std::endl;
}

Socket &Socket::operator=(Socket &&other) {
    if (sockfd > 0) close(sockfd);
    sockfd = other.sockfd;
    other.sockfd = 0;
    std::cout <<"SOCKET MOVE"<<std::endl;
    return *this;
}

void Socket::connect(struct sockaddr_in *addr, unsigned int len) {
    if(::connect(sockfd, reinterpret_cast<struct sockaddr*>(addr), len)!=0)
        throw std::runtime_error("Error during socket connection");
}

ssize_t Socket::read(char *buf, size_t len, int options) {
    ssize_t res = recv(sockfd, buf, len, options);
    if (res < 0) {
        std::cerr << "Error during socket reading: " << strerror(errno) << " | " << errno << std::endl;
        throw std::runtime_error("Error during socket reading string");
    }
    return res;
}

//send const char buffer
ssize_t Socket::write(const char *buf, size_t len, int options) {
    ssize_t res = send(sockfd, buf, len, options);
    if (res < 0) {
        std::cerr << "Error during data sending: " << strerror(errno) << " | " << errno << std::endl;
        throw std::runtime_error("Error during data sending");
    }
    return res;
}

//send integer
ssize_t Socket::write(int* value, size_t len, int options) {
    ssize_t res = send(sockfd, &value, len, options);
    if (res < 0) {
        std::cerr << "Error during data sending: " << strerror(errno) << " | " << errno << std::endl;
        throw std::runtime_error("Error during data sending");
    }
    return res;
}

//recieve integer status
ssize_t Socket::read(int* val, size_t len, int options) {
    ssize_t res = recv(sockfd, val, len, options);
    if (res < 0) {
        std::cerr << "Error during socket reading: " << strerror(errno) << " | " << errno << std::endl;
        throw std::runtime_error("Error during socket reading integer");
    }
    return res;
}

//close socket
void Socket::closeSocket() {
    if (sockfd > 0) {
        std::cout << "Closing socket " << sockfd <<std::endl;
        close(sockfd);
    }
}
