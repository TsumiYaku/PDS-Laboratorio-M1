#pragma once

#include <Socket.h>
#include "ServerSocket.h"
#include <mutex>
#include <thread>
#include <queue>
#include <condition_variable>
#include <iostream>
#include "Connection.h"

// Max number of concurrent connections doing operations on server
#define POOL_SIZE 4
#define PORT 5000

class Server {
    ServerSocket ss;

    // Synchronization structures
    std::mutex fs_m; // mutex to handle filesystem concurrency
    std::mutex pool_m; // mutex to handle connection queue concurrency
    std::condition_variable pool_cv; // condition variable to handle queue concurrency

    // Process pool
    std::queue<Connection> connections; // sockets corresponding to connected clients
    std::vector<std::thread> pool; // thread pool for max number of concurrent connections running

private:
    static void handleConnection(Server* server); // called by threads in the pool during init
    void enqueueConnection(const Connection& conn); // adds a socket to wait in queue
    Connection dequeueConnection(); // remove a socket from waiting in queue

public:
    Server(int port=PORT);

    void run(); // run server indefinitely

};

