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
                freeUsers.push_back(user); // User is again available to receive packets from the select
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
    //

    // Check what message has been received
    // TODO: can't we somehow use a switch? Doesn't allow me with std::string
    if(msg == "CHECK") // If client asks checksum
        sendChecksum(user);
    else if (msg == "SYNC") // If client asks to sync files
        synchronize(user);
    else if (msg == "CREATE" || msg == "MODIFY" || msg == "ERASE") { // Folder listening management
        sendMessage(user, Message("ACK"));
        receiveFile(user);
    }
    std::cout << msg << " OK" << std::endl; 
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
        first = (int)msg.find('_') + 1;
        second = (int)msg.find('_', first);
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

Message Server::awaitMessage(const std::string& user, int msg_size, MessageType type) {
    Socket *socket = &connectedUsers[user];
    //std::cout << socket << std::endl;
    return awaitMessage(socket);
}

Message Server::awaitMessage(Socket* socket, int msg_size, MessageType type) {
    // Socket read
    //std::cout << msg_size << std::endl;
    std::unique_ptr<char[]> buf = std::make_unique<char[]>(msg_size);
    //std::cout << msg_size << std::endl;
    int size;
    socket->read(&size, sizeof(size), 0);
    size = socket->read(buf.get(), size, 0);
    if (size == 0) throw std::runtime_error("Closed socket");

    // Unserialization
    Message m(type);
    try {
        std::stringstream sstream;
        sstream << buf.get();
        Deserializer ia(sstream);
        m.unserialize(ia, 0);
        std::cout <<"DESERIALIZE: " << sstream.str() << std::endl;
    }
    catch (boost::archive::archive_exception& e) {
        throw std::runtime_error(e.what());
    }

    if(m.getType() == MessageType::text)
        std::cout << "\nMessage received: " << m.getMessage() << std::endl;
    else
        std::cout << "\nMessage received" << std::endl;
    return m;
}

void Server::sendMessage(const std::string& user, Message &&m) {
    Socket* socket = &connectedUsers[user];
    sendMessage(socket, std::move(m));
}

void Server::sendMessage(Socket* socket, Message &&m) {
    // Serialization
    std::stringstream sstream;
    try {
        Serializer oa(sstream);
        m.serialize(oa, 0);
        std::cout << "SERIALIZE: " << sstream.str() ;
    }
    catch (boost::archive::archive_exception& e) {
        throw std::runtime_error(e.what());
    }

    // Socket write
    std::string s(sstream.str());
    int length = s.length() + 1;
    socket->write(&length, sizeof(length), 0);
    socket->write(s.c_str(), length, 0);

    if(m.getType() == MessageType::text)
        std::cout << "\nMessage sent: " << m.getMessage() << std::endl;
    else
        std::cout << "\nSending file info: " << m.getFileWrapper().getPath().relative_path()<< std::endl;
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

bool Server::receiveFile(const std::string& user) {
    Folder f(user, user);
    Socket* socket = &connectedUsers[user];

    // Receive command and answer ACK
    Message m = awaitMessage(user);
    sendMessage(user, Message("ACK"));

    // In case we're done receiving a series
    if(m.getMessage().compare("END") == 0) return false;

    // Errors handling
    if(m.getMessage().compare("FS_ERR") == 0) throw std::runtime_error("receiveFile: File system error on client side");
    if(m.getMessage().compare("ERR") == 0) throw std::runtime_error("receiveFile: Generic error on client side");

    // Receive file info
    FileWrapper fileInfo = awaitMessage(user, SIZE_MESSAGE_TEXT, MessageType::file).getFileWrapper();
    sendMessage(user, Message("ACK"));

    // Check if the received message was a Directory or a File
    if(m.getMessage().compare("DIRECTORY") == 0 )
        switch (fileInfo.getStatus()) {
        case FileStatus::modified : // If modified it first deletes the folder, then re-creates it (so no break)
            //f.deleteFile(fileInfo.getPath());
        case FileStatus::created :
            f.writeDirectory(fileInfo.getPath());
            break;
        case FileStatus::erased :
            f.deleteFile(fileInfo.getPath());
            break;
        default:
            break;
        }
    else if(m.getMessage().compare("FILE") == 0 ) {
        switch (fileInfo.getStatus()) {
            case FileStatus::modified : // If modified it first deletes the folder, then re-creates it (so no break)
                f.deleteFile(fileInfo.getPath());
            case FileStatus::created : {
                std::cout << "Reading blocks from socket" << std::endl;
                // Read chunks of data
                int count_char = fileInfo.getSize();
                int num = 0;
                while (count_char > 0) {
                    std::cout << "Remaining data: " << count_char << std::endl;
                    num = count_char > SIZE_MESSAGE_TEXT ? SIZE_MESSAGE_TEXT : count_char;
                    std::unique_ptr<char[]> buf = std::make_unique<char[]>(num);
                    int receivedSize = socket->read(buf.get(), num, 0);
                    f.writeFile(fileInfo.getPath(), buf.get(), receivedSize);
                    count_char -= receivedSize;
                    //sendMessage(user, Message("ACK"));
                }
                break;
            }
            case FileStatus::erased :
                f.deleteFile(fileInfo.getPath());
                break;
            default:
                break;
        }
    }
    else if (m.getMessage().compare("FILE_DEL") == 0) {
        if(!fileInfo.getPath().empty())
            f.deleteFile(fileInfo.getPath());
    }

    // Send ACK to indicate operation success
    sendMessage(user, Message("ACK"));
    return true;
}

void Server::sendFile(const std::string& user, const filesystem::path& path) {
    Folder f(user, user);
    filesystem::path filePath = f.getPath()/path;
    Socket *socket = &connectedUsers[user];

    // Sending command message
    std::string command;
    int size = 0;
    if(filesystem::is_directory(filePath))
        command = "DIRECTORY";
    else if(filesystem::is_regular_file(filePath)) {
        command = "FILE";
        size = f.getFileSize(path);
    }
    else if(!filesystem::exists(filePath))
        command = "FILE_DEL";
    else { // in case of any error
        sendMessage(user, Message("ERR"));
        throw std::runtime_error("File not supported");
    }
    sendMessage(user, Message(std::move(command)));
    Message m = awaitMessage(user); // waiting for ACK

    // Sending File Info
    FileWrapper fileInfo = FileWrapper(path, FileStatus::modified, size);//path, status, size file
    sendMessage(user, Message(std::move(fileInfo)));
    m = awaitMessage(user); // waiting fileInfo ACK

    // if it's a file, also send the data
    if(is_regular_file(filePath)){
        filesystem::ifstream file; // file to send
        file.open(filePath, std::ios::in | std::ios::binary);
        int count_char = size; // remaining size to send
        int num; // size of packet to write into the socket
        while(count_char > 0){
            // Init buffer
            num = count_char > SIZE_MESSAGE_TEXT ? SIZE_MESSAGE_TEXT : count_char;
            std::unique_ptr<char[]> buf = std::make_unique<char[]>(num);

            // Throw error if it can't read the file
            if(!file.read(buf.get(), num)){
                sendMessage(user, Message("FS_ERR"));
                m = awaitMessage(user);
                throw std::runtime_error("Impossible read file");
            };

            // Socket sending
            socket->write(buf.get(), num, 0);
            count_char -= num;
        }
        file.close();
    }

    m = awaitMessage(user); //waiting final ACK
}

void Server::downloadDirectory(const std::string& user) {
    Folder f(user, user);
    f.wipeFolder(); // remove all files present in the folder since we're receiving updated ones

    std::cout << "Waiting directory from user " << user << std::endl;

    // Keep waiting for files
    while(receiveFile(user)) {}
    sendMessage(user, Message("ACK"));
}

void Server::uploadDirectory(const std::string& user) {
    Folder f(user, user);

    // Send all files
    for(filesystem::path path: f.getContent())
        sendFile(user, path);

    // Signal end of file upload and await ACK
    sendMessage(user, Message("END"));
    Message m = awaitMessage(user);
}
