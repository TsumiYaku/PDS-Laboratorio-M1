#pragma once

#include <Socket.h>
#include "ServerSocket.h"
#include <mutex>
#include <thread>
#include <queue>
#include <condition_variable>
#include <iostream>
#include <packets/Message.h>

// Max number of concurrent connections doing operations on server
#define POOL_SIZE 4
#define PORT 5000

class Server {
    ServerSocket ss;

    // Synchronization structures
    std::mutex fs_m; // mutex to handle filesystem concurrency
    std::mutex pool_m; // mutex to handle connection queue concurrency
    std::condition_variable pool_cv; // condition variable to handle queue concurrency

    // Thread pool management
    std::vector<std::thread> pool; // thread pool for max number of concurrent connections running
    std::queue<std::pair<std::string, Message>> packetsQueue;

    std::map<std::string, Socket> connectedUsers; // sockets corresponding to connected clients
    std::list<std::string> freeUsers; // users busy with their own interactions

private:
    std::string handleLogin(Socket*); // log user into the server
    void parsePacket(std::pair<std::string, Message>); // parse the received packets and act accordingly

    // Pool queue methods
    void enqueuePacket(std::pair<std::string, Message>); // adds a packet to the pool's queue
    std::pair<std::string, Message> dequeuePacket(); // called by threads in the pool to consume packets

    // Communication
    Message awaitMessage(const std::string& user); // wait to read a Message packet
    Message awaitMessage(Socket*);
    void sendMessage(const std::string&, Message&&); // send a Message Packet
    void sendMessage(Socket*, Message&&);

    // Folder management
    void sendChecksum(std::string user); // send the checksum associated to the user's folder
    void synchronize(std::string user); // synchronize the user's folder
    bool receiveFile(std::string user); // wait for file from user
    void downloadDirectory(std::string user); // download user's directory
    void uploadDirectory(std::string user); // send the directory to the user

public:
    Server(int port=PORT); 
    void run(); // run server indefinitely
};

