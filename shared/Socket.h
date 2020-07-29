#pragma once

#include <sys/types.h>
#include <sys/socket.h>
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
    Socket &operator=(Socket &&other);

    // Operations
    ssize_t read(char* buf, size_t len, int options);
    ssize_t write(char* buf, size_t len, int options);
    void connect(struct sockaddr_in *addr, unsigned int len);

};
