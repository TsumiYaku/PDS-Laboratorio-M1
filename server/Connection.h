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

private:
    Message awaitMessage();
    void handlePacket(Message m); // handle messages sent by client
    void sendChecksum();
    void sendResponse(const std::string& r);
    void receiveFiles();
    void sendFiles();

public:
    Connection(Socket socket);

    void run();

};


