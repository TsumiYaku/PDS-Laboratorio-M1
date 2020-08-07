#pragma once
#include <Socket.h>
#include <packets/Message.h>
#include <packets/FileWrapper.h>
#include <Folder.h>
#include <mutex>
#include <list>

class Connection {
    Socket socket;
    std::string username;
    Folder *f;
    bool logged = false;
    bool terminate = false;
    static std::list<std::string> users;
    static std::mutex dbLock;

private:
    Message awaitMessage(size_t);
    void handlePacket(Message&&); // handle messages sent by client
    void sendChecksum();
    void sendMessage(Message&&);
    void synchronize();
    void downloadDirectory();
    void uploadDirectory();
    bool receiveFile();
    void listenPackets();

public:
    Connection(Socket&& socket);
    Connection(const Connection&) = delete;
    Connection(Connection&&);
    Connection& operator=(Connection&&);
    ~Connection();

    void run();

};


