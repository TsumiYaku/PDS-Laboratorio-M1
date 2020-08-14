#include "Server.h"
#include <Folder.h>
#include <iostream>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <libnet.h>
#include <unistd.h>
#include "AuthManager.h"
#include <communication/Exchanger.h>

using Serializer = boost::archive::text_oarchive;
using Deserializer = boost::archive::text_iarchive;

Server::Server(int port): ss(port) {
    // Process pool initialization
    for(int i=0; i<POOL_SIZE; i++) {
        std::thread t([this]() -> void {
            //std::lock_guard<std::mutex> lg()
            std::string user;
            while (true) {
                std::pair<std::string, Message> packet = this->dequeuePacket(); // Blocking read of the queue
                user = packet.first;
                try {
                    parsePacket(std::move(packet));
                     std::cout << "PARSE CREATE SERVER" << std::endl;
                }
                catch (std::runtime_error& e) {
                    std::cout << e.what() << std::endl;
                    connectedUsers.erase(user); // Break connection with user if there are problems
                }
                this->userlist_m.lock();
                freeUsers.push_back(user); // User is again available to receive communication from the select
                this->userlist_m.unlock();
            }
        });
        t.detach();
        pool.push_back(std::move(t));
    }
}

void Server::run() {
    std::cout << "Server ready, waiting for connections..." << std::endl;

    fd_set socketSet;
    int maxFd = 0;
    int activity;
    struct timeval tv;

    while(true) {
        maxFd = 0;
        FD_ZERO(&socketSet); // reset socket set
        ss.addToSet(socketSet, maxFd); // add sever to the set

        // Reset timeout value
        tv.tv_sec = 2; // Every 2 seconds select will go on timeout to check if new connections are free again
        tv.tv_usec = 0;

        // add non-busy clients to the listening set
        userlist_m.lock(); // Locking userlist to avoid changes to the list during iteration

        for(const auto& user: freeUsers)
            connectedUsers[user].addToSet(socketSet, maxFd);

        userlist_m.unlock();

        // Listen to socket changes
        activity = select(maxFd+1, &socketSet, NULL, NULL, &tv);
        if((activity < 0) && (errno!=EINTR)) std::cout << "Select error" << std::endl;

        // Check who sent a request on their socket
        userlist_m.lock(); // Locking userlist to avoid changes to the list during iteration
        for(const auto &user: freeUsers) {
            if(connectedUsers[user].isSet(socketSet)) {
                std::cout << "CC ISSET " << std::endl;
                Message m(MessageType::text);
                // Check if socket has no errors or hasn't disconnected
                try {
                    std::cout << "AWAIT BEFORE " << std::endl;
                    m = awaitMessage(user);
                    std::cout << "AWAIT AFTER " << std::endl;
                }
                catch (std::runtime_error& e) {
                    std::cout << e.what() << std::endl;
                    connectedUsers.erase(user);
                    freeUsers.remove(user);
                    break;
                }

                // Else, it's a good communication that has to be handled
                freeUsers.remove(user); // User is now busy

                /* MULTI THREAD EXECUTION, comment it if you need a single thread for debugging */
                enqueuePacket(std::pair<std::string, Message>(user, std::move(m))); // Feed packet to the pool

                /* SINGLE THREAD EXECUTION, uncomment the following lines if you need for debugging */
                /*
                try {
                    parsePacket(std::pair<std::string, Message>(user, std::move(m)));
                }
                catch (std::runtime_error& e) {
                    std::cout << e.what() << std::endl;
                    connectedUsers.erase(user); // Break connection with user if there are problems
                }
                freeUsers.push_back(user);
                */
                break;
            }
        }
        userlist_m.unlock();

        // If it was none of the clients, check if ServerSocket received a new connection
        if (ss.isSet(socketSet)) {
            // Read incoming connection
            struct sockaddr_in addr;
            unsigned int len = sizeof(addr);
            Socket s = ss.accept(&addr, &len);
            char name[16];
            if(inet_ntop(AF_INET, &addr.sin_addr, name, sizeof(name)) == nullptr) throw std::runtime_error("Error during address name conversion");
            std::cout << "Client " << name << ":" << ntohs(addr.sin_port) << " connected" << std::endl;

            // Authenticate user
            std::string user;
            try {
                user = handleLogin(&s);
            }
            catch (std::runtime_error& e) {
                std::cout << "Error during user login" << std::endl;
                std::cout << "runtime_error: " << e.what() << std::endl;
            }
            if(user != "") {
                std::cout << "Connected user: " << user << std::endl;
                connectedUsers.insert(std::pair<std::string, Socket>(user, std::move(s)));
                userlist_m.lock();
                freeUsers.push_back(user);
                userlist_m.unlock();
            }
        }
    }
}

void Server::enqueuePacket(std::pair<std::string, Message> packet) {
    std::lock_guard<std::mutex> lg(pool_m);
    packetsQueue.push(std::move(packet));
    pool_cv.notify_all(); // notify threads that a new connection is ready to be handled
}

std::pair<std::string, Message> Server::dequeuePacket() {
    std::unique_lock<std::mutex> lg(pool_m);
    while(packetsQueue.empty()) // wait for a connection to handle
        pool_cv.wait(lg);

    std::pair<std::string, Message> packet = std::move(packetsQueue.front());
    packetsQueue.pop();
    return packet;
}

void Server::parsePacket(std::pair<std::string, Message> packet) {
    std::string user = std::move(packet.first);
    Message m = std::move(packet.second);

    std::string msg(m.getMessage());
    std::string response;

    // Check what message has been received
    if(msg == "CHECK") // If client asks checksum
        sendChecksum(user);
    else if (msg == "SYNC") // If client asks to sync files
        synchronize(user);
    else if (msg == "CREATE" || msg == "MODIFY" || msg == "ERASE") { // Folder listening management
        Folder f(user, user);
        sendMessage(user, Message("ACK"));
        FileExchanger::receiveFile(&connectedUsers[user], &f);
    }
    std::cout << msg << " OK" << std::endl; 
}

std::string Server::handleLogin(Socket* sock) {
    std::string user;
    std::string pass;

    // Initially awaits login
    Message m = MessageExchanger::awaitMessage(sock);

    // If it's not a login message...
    if(m.getMessage().find("LOGIN") == std::string::npos) {
        MessageExchanger::sendMessage(sock, Message("NOT_OK"));
        return "";
    }
    else {
        // Extract username from login msg
        std::string msg = m.getMessage();
        int first, second;
        first = (int)msg.find('_') + 1;
        second = (int)msg.find('_', first);
        user = msg.substr(first, second - first);
        pass = msg.substr(second+1);

        // Check if user is already logged
        std::map<std::string, Socket>::iterator it;
        for (it = connectedUsers.begin(); it != connectedUsers.end(); it++)
            if (it->first == user) {
                std::cout << "User already logged" << std::endl;
                MessageExchanger::sendMessage(sock, Message("NOT_OK"));
                return "";
            }
    }

    // DB authentication
    bool success = AuthManager::tryLogin(user, pass);
    if (success)
        MessageExchanger::sendMessage(sock, Message("OK"));
    else {
        MessageExchanger::sendMessage(sock, Message("NOT_OK"));
        return "";
    }

    // All checks passed
    return user;
}

Message Server::awaitMessage(const std::string& user, int msg_size, MessageType type) {
    Socket *socket = &connectedUsers[user];
    //std::cout << socket << std::endl;
    return MessageExchanger::awaitMessage(socket);
}

void Server::sendMessage(const std::string& user, Message &&m) {
    Socket* socket = &connectedUsers[user];
    MessageExchanger::sendMessage(socket, std::move(m));
}

void Server::sendChecksum(const std::string& user) {
    Folder f(user, user);
    int data = (int)f.getChecksum();
    //std::cout << "SEND CHECKSUM: " << data << std::endl;
    ssize_t size = connectedUsers[user].write(&data, sizeof(data), 0);
}

void Server::synchronize(const std::string& user) {
    sendChecksum(user);

    Message m = awaitMessage(user);
    std::string msg(m.getMessage());

    if(msg == "OK")
        return;
    else if(msg == "UPDATE") {
        sendMessage(user, Message("ACK"));
        downloadDirectory(user);
        std::cout << "UPDATE SUCCESS"<<std::endl;
    }
    else if(msg == "DOWNLOAD") {
        sendMessage(user, Message("ACK"));
        uploadDirectory(user);
        std::cout << "DOWNLOAD SUCCESS"<<std::endl;
    }
}

void Server::downloadDirectory(const std::string& user) {
    Folder f(user, user);
    f.wipeFolder(); // remove all files present in the folder since we're receiving updated ones

    std::cout << "Waiting directory from user " << user << std::endl;

    // Keep waiting for files
    while(FileExchanger::receiveFile(&connectedUsers[user], &f)) {}
    sendMessage(user, Message("ACK"));
}

void Server::uploadDirectory(const std::string& user) {
    Folder f(user, user);

    // Send all files
    for(filesystem::path path_: f.getContent())
        FileExchanger::sendFile(&connectedUsers[user], &f, f.removeFolderPath(path_.string()), FileStatus::modified);

    // Signal end of file upload and await ACK
    sendMessage(user, Message("END"));
    Message m = awaitMessage(user);
}
