#include "Socket.h"
#include <iostream>
#include <unistd.h>
#include <stdexcept>
#include <string.h>
#include <netinet/in.h>
#include <errno.h>
#include<sys/sendfile.h>

Socket::Socket(int sockfd): sockfd(sockfd) {
    if (sockfd < 0) throw std::runtime_error("Error during socket creation");
    std::cout << "Socket " << sockfd << " initialized" << std::endl;
}

Socket::Socket() {
    sockfd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) throw std::runtime_error("Error during socket creation");
    std::cout << "Socket " << sockfd << " initialized" << std::endl;
}

Socket::~Socket() {
    if (sockfd > 0) {
        std::cout << "Closing socket " << sockfd <<std::endl;
        close(sockfd);
        sockfd = 0;
    }
}

Socket::Socket(Socket &&other) {
    if (sockfd > 0) close(sockfd);
    sockfd = other.sockfd;
    other.sockfd = 0;
    std::cout <<"SOCKET MOVE"<<std::endl;
}

Socket &Socket::operator=(Socket &&other) {
    if (sockfd > 0) close(sockfd);
    sockfd = other.sockfd;
    other.sockfd = 0;
    std::cout <<"SOCKET MOVE OPERATOR="<<std::endl;
    return *this;
}

int Socket::getSocket(){
    return sockfd;
}

void Socket::connect(struct sockaddr_in *addr, unsigned int len) {
    if(::connect(sockfd, reinterpret_cast<struct sockaddr*>(addr), len)!=0)
        throw std::runtime_error("Error during socket connection");
}

ssize_t Socket::read(char *buf, size_t len, int options) {
    ssize_t res = recv(sockfd, buf, len, options);
    if (res < 0) {
        throw std::runtime_error("Error during socket reading string");
    }
    if (res == 0) {
        throw std::runtime_error("Socket Closed");
    }
    return res;
}

//send const char buffer
ssize_t Socket::write(const char *buf, size_t len, int options) {
    ssize_t res = send(sockfd, buf, len, options);
    if (res < 0) {
        throw std::runtime_error("Error during data sending");
    }
    if (res == 0) {
        throw std::runtime_error("Socket Closed");
    }
    return res;
}

//send integer
ssize_t Socket::write(int* value, size_t len, int options) {
    ssize_t res = send(sockfd, value, len, options);
    if (res < 0) {
        throw std::runtime_error("Error during data sending");
    }
    if (res == 0) {
        throw std::runtime_error("Socket Closed");
    }
    return res;
}

//recieve integer status
ssize_t Socket::read(int* val, size_t len, int options) {
    ssize_t res = recv(sockfd, val, len, options);
    if (res < 0) {
        throw std::runtime_error("Error during socket reading integer");
    }
    if (res == 0) {
        throw std::runtime_error("Socket Closed");
    }
    return res;
}

void Socket::print() {
        std::cout << "PRINT socket " << sockfd <<std::endl;
}

//close socket
void Socket::closeSocket() {
    if (sockfd > 0) {
        std::cout << "Closing socket " << sockfd <<std::endl;
        close(sockfd);
        sockfd = 0;
    }
}

void Socket::addToSet(fd_set& set, int& maxFd) {
    FD_SET(sockfd, &set);
    if(sockfd > maxFd)
        maxFd = sockfd;
}

void Socket::removeFromSet(fd_set& set) {
    FD_CLR(sockfd, &set);
}

bool Socket::isSet(fd_set& set) {
    return FD_ISSET(sockfd, &set);
}
