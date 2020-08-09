#include "Server.h"
#include <Folder.h>
#include <iostream>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <libnet.h>
#include <unistd.h>

using Serializer = boost::archive::text_oarchive;
using Deserializer = boost::archive::text_iarchive;

Server::Server(int port): ss(port) {
    for(int i=0; i<POOL_SIZE; i++) {
        std::thread t([this]() -> void {
            std::string user;
            while (true) {
                std::pair<std::string, Message> packet = this->dequeuePacket();
                user = packet.first;
                parsePacket(std::move(packet));
                freeUsers.push_back(user);
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

    while(true) {
        maxFd = 0;

        FD_ZERO(&socketSet); // reset socket set
        ss.addToSet(socketSet, maxFd); // add sever to the set

        // add non-busy clients to the listening set
        for(const auto& user: freeUsers)
            connectedUsers[user].addToSet(socketSet, maxFd);

        // Listen to socket changes
        activity = select(maxFd+1, &socketSet, NULL, NULL, NULL);
        if((activity < 0) && (errno!=EINTR)) std::cout << "Select error" << std::endl;

        // Check who sent a request on their socket
        for(const auto &user: freeUsers) {
            if(connectedUsers[user].isSet(socketSet)) {
                Message m(MessageType::text);
                // Check if socket has no errors or hasn't disconnected
                try {
                    m = awaitMessage(user);
                }
                catch (std::runtime_error& e) {
                    std::cout << e.what() << std::endl;
                    connectedUsers.erase(user);
                    freeUsers.remove(user);
                    continue;
                }

                // Else, it's a good communication that has to be handled
                freeUsers.remove(user); // User is now busy
                enqueuePacket(std::pair<std::string, Message>(user, std::move(m))); // Feed packet to the pool
            }
        }

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
            std::string user = handleLogin(&s);
            if(user != "") {
                connectedUsers.insert(std::pair<std::string, Socket>(user, std::move(s)));
                freeUsers.push_back(user);
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
    //std::cout << msg << std::endl;

    // Check what message has been received
    // TODO: can't we somehow use a switch? Doesn't allow me with std::string
    if(msg == "CHECK") // If client asks checksum
        sendChecksum(user);
    else if (msg == "SYNC") // If client asks to sync files
        synchronize(user);
    else if (msg == "CREATE" || msg == "MODIFY" || msg == "ERASE") { // Folder listening management
        sendMessage(user, Message("ACK"));
        while(receiveFile(user)) {};
    }
}

std::string Server::handleLogin(Socket* sock) {
    std::string user;

    // Initially awaits login
    Message m = awaitMessage(sock);

    // If it's not a login message...
    if(m.getMessage().find("LOGIN") == std::string::npos)
        sendMessage(sock, Message("NOT_OK"));
    else {
        // Extract username from login msg
        std::string msg = m.getMessage();
        int first, second;
        first = msg.find('_') + 1;
        second = msg.find('_', first);
        user = msg.substr(first, second - first);

        // Check if user is already logged
        std::map<std::string, Socket>::iterator it;
        for (it = connectedUsers.begin(); it != connectedUsers.end(); it++)
            if (it->first == user) {
                std::cout << "User already logged" << std::endl;
                sendMessage(sock, Message("NOT_OK"));
                user = "";
            }
    }

    // TODO: authentication with password

    // All checks passed
    sendMessage(sock, Message("OK"));
    return user;
}

Message Server::awaitMessage(const std::string& user) {
    Socket *socket = &connectedUsers[user];
    std::cout << "Received message from user: " << user << std::endl;
    return awaitMessage(socket);
}

Message Server::awaitMessage(Socket* socket) {
    size_t msg_size = SIZE_MESSAGE_TEXT;

    // Socket read
    char buf[msg_size];
    int size = socket->read(buf, msg_size, 0);
    if (size == 0) throw std::runtime_error("Closed socket");

    //std::cout << buf << " " << size << std::endl;
    // Unserialization
    std::stringstream sstream;
    sstream << buf;
    Deserializer ia(sstream);
    Message m(MessageType::text);
    m.unserialize(ia, 0);

    if(m.getType() == MessageType::text)
        std::cout << "Message received: " << m.getMessage() << std::endl;
    else
        std::cout << "Incoming file..." << m.getFileWrapper().getPath().relative_path()<< std::endl;
    return m;
}

void Server::sendMessage(const std::string& user, Message &&m) {
    Socket* socket = &connectedUsers[user];
    std::cout << "Sending message to user: " << user << std::endl;
    sendMessage(socket, std::move(m));
}

void Server::sendMessage(Socket* socket, Message &&m) {
    // Serialization
    std::stringstream sstream;
    Serializer oa(sstream);
    m.serialize(oa, 0);

    // Socket write
    std::string s(sstream.str());
    socket->write(s.c_str(), strlen(s.c_str())+1, 0);

    if(m.getType() == MessageType::text)
        std::cout << "Message sent: " << m.getMessage() << std::endl;
    else
        std::cout << "Sending file..." << m.getFileWrapper().getPath().relative_path()<< std::endl;
}

void Server::sendChecksum(std::string user) {
    Folder f(user, user);
    int data = (int)f.getChecksum();
    //std::cout << "SEND CHECKSUM: " << data << std::endl;
    ssize_t size = connectedUsers[user].write(&data, sizeof(data), 0);
}

void Server::synchronize(std::string user) {
    sendChecksum(user);

    Message m = awaitMessage(user);
    std::string msg(m.getMessage());

    if(msg == "OK")
        return;
    else if(msg == "UPDATE") {
        sendMessage(user, Message("ACK"));
        downloadDirectory(user);
    }
    else if(msg == "DOWNLOAD") {
        sendMessage(user, Message("ACK"));
        uploadDirectory(user);
    }
}

bool Server::receiveFile(std::string user) {
    Folder f(user, user);

    std::stringstream ss;
    Message m = awaitMessage(user);
    sendMessage(user, Message("OK"));

    if(m.getMessage().compare("END") == 0) return false;
    if(m.getMessage().compare("FS_ERR") == 0) return false;
    if(m.getMessage().compare("ERR") == 0) return false;

    char* buf  = new char[SIZE_MESSAGE_TEXT];
    //recieve message FILE and after recieve filewrapper

    if(m.getMessage().compare("DIRECTORY") == 0 )
    {
        connectedUsers[user].read(buf, SIZE_MESSAGE_TEXT, 0);
        ss << buf;
        Deserializer ia(ss);
        Message m = Message(MessageType::file);
        m.unserialize(ia, 0);

        FileWrapper file = m.getFileWrapper();
        std::cout << "Receiving directory: " << file.getPath() <<std::endl;
        switch (file.getStatus()) {
            case FileStatus::created: {
                f.writeDirectory(file.getPath());
                break;
            }
            case FileStatus::modified: {
                f.writeDirectory(file.getPath());
                break;
            }
            case FileStatus::erased: {
                f.deleteFile(file.getPath());
                break;
            }
            default:
                break;
        }
    }
    else if(m.getMessage().compare("FILE") == 0 ){
        int size_text_serialize = 0;
        connectedUsers[user].read(&(size_text_serialize), sizeof(size_text_serialize), 0);
        sendMessage(user, Message("ACK"));
        buf = new char[size_text_serialize];
        connectedUsers[user].read(buf, size_text_serialize, 0);
        ss << buf;
        std::cout <<"BUFFER SIZE:" << size_text_serialize << "\n" << ss.str() << std::endl;
        Deserializer ia(ss);
        Message m = Message(MessageType::file);
        m.unserialize(ia, 0);
        FileWrapper file = m.getFileWrapper();
        std::cout << " FILE " << file.getPath() <<std::endl;
        switch (file.getStatus()) {
            case FileStatus::created: {
                char * data = strdup(file.getData());
                f.writeFile(file.getPath(), data, strlen(data));
                break;
            }
            case FileStatus::modified: {
                char *data = strdup(file.getData());
                f.writeFile(file.getPath(), data, strlen(data));
                break;
            }
            case FileStatus::erased: {
                f.deleteFile(file.getPath());
                break;
            }
            default:
                break;
        }
    }
    else{//FILE_DEL
        buf = new char[SIZE_MESSAGE_TEXT];
        connectedUsers[user].read(buf, SIZE_MESSAGE_TEXT, 0);
        ss << buf;
        Deserializer ia(ss);
        Message m = Message(MessageType::file);
        m.unserialize(ia, 0);
        FileWrapper file = m.getFileWrapper();
        std::cout << " FILE DELETED " << file.getPath() <<std::endl;
        if(file.getPath().string().compare("") != 0)
            f.deleteFile(file.getPath());
    }

    sendMessage(user, Message("ACK"));
    return true;
}

void Server::downloadDirectory(std::string user) {
    std::cout << "Waiting directory from user " << user << std::endl;
    char buf[SIZE_MESSAGE_TEXT];
    std::stringstream ss;
    while(receiveFile(user)) {}
}

void Server::uploadDirectory(std::string user) {
    Folder f(user, user);
    std::cout << "Sending directory to user " << user << std::endl;

    for (const filesystem::path& path : f.getContent()) {

        if(filesystem::is_directory(path)) {
            sendMessage(user, Message("DIRECTORY"));
            Message m = awaitMessage(user);
            //std::cout << f->strip_root(path).string() << std::endl;
            FileWrapper file = FileWrapper(f.strip_root(path), strdup(""), FileStatus::created); // TODO: check if 'created' is the right status
            Message m2 = Message(std::move(file));
            std::cout << "DIRECTORY SEND " << m2.getFileWrapper().getPath() << std::endl;
            sendMessage(user, std::move(m2));
        }else{
            int size = (int)f.getFileSize(f.removeFolderPath(path.string()));
            //std::cout << size <<std::endl;
            char* buf = new char[size+1];

            if(!f.readFile(f.removeFolderPath(path.string()), buf, size)) {
                sendMessage(user, Message("FS_ERR"));
                break;
            }
            std::cout <<"CONTENT FILE " << buf << std::endl;

            sendMessage(user, Message("FILE"));
            Message m = awaitMessage(user); //attendo un response da client


            FileWrapper file = FileWrapper(filesystem::path(f.removeFolderPath(path.string())), buf, FileStatus::created); // TODO: check if 'created' is the right status

            Message m2 = Message(std::move(file)); //invio file
            std::stringstream ss;
            Serializer ia(ss);
            m2.serialize(ia, 0);
            std::string s(ss.str());
            size = s.length() + 1;
            connectedUsers[user].write(&(size), sizeof(size), 0);//invio taglia testo da deserializzare
            m = awaitMessage(user); //attendo ACK
            connectedUsers[user].write(s.c_str(), strlen(s.c_str())+1, 0);//invio testo serializzato
            //m2.print();
            //sendMessage(std::move(m2));

            m = awaitMessage(user);   //attendo response (TODO BETTER)

        }
    }
    sendMessage(user, Message("END"));
}
