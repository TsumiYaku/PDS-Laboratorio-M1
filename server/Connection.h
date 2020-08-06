#pragma once
#include <Socket.h>
#include <packets/Message.h>
#include <packets/FileWrapper.h>
#include "Folder.h"
#include <mutex>

class Connection {
    Socket socket;
    std::string username;
    Folder *f;
    bool logged = false;
    bool terminate = false;

private:
    
    Message awaitMessage(size_t);
    void handlePacket(Message&&); // handle messages sent by client
    void sendChecksum();
    void sendMessage(Message&&);
    void synchronize();
    void receiveDirectory();
    void sendDirectory();
    bool receiveFile();
    void listenPackets();

public:
    Connection(Socket&& socket);
    Connection(const Connection&) = delete;
    Connection(Connection&&);
    Connection& operator=(Connection&&);

    void run();

};


