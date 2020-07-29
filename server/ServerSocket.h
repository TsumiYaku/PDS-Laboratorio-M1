#pragma once

#include <Socket.h>

class ServerSocket: private Socket {
public:
    ServerSocket(int port);

    Socket accept(struct sockaddr_in* addr, unsigned int* len);
};
