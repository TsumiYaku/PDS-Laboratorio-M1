#include "Socket.h"
#include <iostream>
#include <unistd.h>
#include <stdexcept>
#include <string.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include<sys/sendfile.h>
#include <fcntl.h>

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
    if (res < 0) throw std::runtime_error("Error during socket reading string");
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

/**************************************************************************/

//close socket
void Socket::closeSocket() {
    if (sockfd > 0) {
        std::cout << "Closing socket " << sockfd;
        close(sockfd);
    }
}

//send const char buffer
ssize_t Socket::write(const char *buf, size_t len, int options) {
    ssize_t res = send(sockfd, buf, len, options);
    if (res < 0) throw std::runtime_error("Error during data sending");
    return res;
}

//recieve integer status
ssize_t Socket::readInt(int* buf, size_t len, int options) {
    ssize_t res = recv(sockfd, buf, len, options);
    if (res < 0) throw std::runtime_error("Error during socket reading integer");
    return res;
}


//send file  client -> server
ssize_t Socket::writeFile(std::string pathFile, int options) {
    struct stat obj;
    int size, filehandle;
     filehandle = open(pathFile.c_str(), O_RDONLY);
    if(filehandle==-1){
        return -1;
    }
    stat(pathFile.c_str(), &obj); 
    size = obj.st_size;
    ssize_t res = send(sockfd, &size, sizeof(int), options);
    if (res < 0) throw std::runtime_error("Error during size file sending");
    ssize_t res = sendfile(sockfd, filehandle, NULL, size);
    if (res < 0) throw std::runtime_error("Error during file sending");
    return res;
}

//create/modifiy file server recieved from client
int Socket::readFile(std::string filename) {
    int ret = 0;
    int size;
    int fileHandle;
	char *buf;
	recv(sockfd, &size, sizeof(int), 0);
    
    fileHandle = open(filename.c_str(), O_CREAT | O_EXCL | O_WRONLY, 0666);
    if(fileHandle == -1)
    {
        throw std::runtime_error("Error during file reading");
    }

	buf = (char*) malloc(sizeof(char*)*size);

	recv(sockfd, buf, size, 0);
	ret = ::write(fileHandle, buf, size);
	close(fileHandle);
    
	send(sockfd, &ret, sizeof(int), 0);
    free(buf);

    return ret;

}




