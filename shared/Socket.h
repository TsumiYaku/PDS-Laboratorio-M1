#pragma once

#include <sys/types.h>
#include <sys/socket.h>
#include <string>
#include <netinet/in.h>

class Socket {
    Socket(const Socket &) = delete;
    Socket &operator=(const Socket &) = delete;

protected:
    int sockfd;

public:
    // Constructors, destructors and movers
    Socket();
    Socket(int sockfd);
    ~Socket();
    Socket(Socket &&other);
    void closeSocket();
    Socket &operator=(Socket &&other);
    
    /* Operations */
    void connect(struct sockaddr_in *addr, unsigned int len);

    // Socket read
    ssize_t read(char* buf, size_t len, int options);
    ssize_t read(int* val, size_t len, int options);

    // Socket write
    ssize_t write(const char* buf, size_t len, int options);
    ssize_t write(int* value, size_t len, int options);

};
