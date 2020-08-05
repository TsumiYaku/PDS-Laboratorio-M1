#include "Connection.h"

Connection::Connection(Socket&& socket) {
     this->socket = std::move(socket);
}

Message Connection::awaitMessage(size_t msg_size = 1024) {
    // Socket read
    char buf[msg_size];
    int size = socket.read(buf, msg_size, 0);
    
    std::cout << buf << " " << size << std::endl;
    // Unserialization
    std::stringstream sstream;
    sstream << buf;
    Deserializer ia(sstream);
    Message m(MessageType::text);
    m.unserialize(ia, 0);

    if (logged)
        std::cout << "Received message from user: " << username << std::endl;
    if(m.getType() == MessageType::text)
        std::cout << "Message: " << m.getMessage() << std::endl;
    else
        std::cout << "Incoming file..." << std::endl;


    return m;
}

void Connection::sendMessage(Message &&m) {
    if (logged)
        std::cout << "Sending message to user: " << username << std::endl;
    if(m.getType() == MessageType::text)
        std::cout << "Message: " << m.getMessage() << std::endl;
    else
        std::cout << "Sending file..." << std::endl;

    // Serialization
    std::stringstream sstream;
    Serializer oa(sstream);
    m.serialize(oa, 0);

    // Socket write
    std::string s(sstream.str());
    socket.write(s.c_str(), strlen(s.c_str())+1, 0);
}

void Connection::run() {
    // Initially awaits login
    Message m = awaitMessage();

    // If it's not a login message...
    // TODO: implement better auth
    if(m.getMessage().find("LOGIN") == std::string::npos) {
        sendMessage(Message("NOT_OK"));
        return;
    } else {
        sendMessage(Message("OK"));

        // Extract username from login msg
        std::string msg = m.getMessage();
        int first, second;
        first = msg.find("_") + 1;
        second = msg.find("_", first);
        username = msg.substr(first, second-first);
        logged = true;
        f = new Folder(username, username);

        std::cout << "User " << username << " successfully authenticated" << std::endl;
    }

    listenPackets();
}

void Connection::listenPackets() {
    while(true) {
        if (terminate || !logged)
            return;

        // Await further messages
        Message m = awaitMessage();

        // Client can't simply send files, must first send a message
        if(m.getType() == MessageType::file)
            sendMessage(Message("ERR"));
        else
            handlePacket(std::move(m));
    }
}


void Connection::handlePacket(Message &&m) {
    std::string msg(m.getMessage());
    std::string response;
    std::cout << msg << std::endl;

    // Check what message has been received
    // TODO: can't we somehow use a switch? Doesn't allow me with std::string
    if(msg == "CHECK") // If client asks checksum
        sendChecksum();
    else if (msg == "SYNC") // If client asks to sync files
        synchronize();
    else if (msg == "CREATE" || msg == "MODIFY" || msg == "ERASE") { // Folder listening management
        sendMessage(Message("ACK"));
        receiveFile();
    }

}

void Connection::synchronize() {
    sendChecksum();

    Message m = awaitMessage();
    std::string msg(m.getMessage());

    if(msg == "OK")
        return;
    else if(msg == "UPDATE") {
        sendMessage(Message("ACK"));
        receiveDirectory();
    }
    else if(msg == "DOWNLOAD") {
        sendMessage(Message("ACK"));
        sendDirectory();
    }
}

void Connection::sendChecksum() {
    int data = (int)f->getChecksum();
    ssize_t size = socket.write(&data, sizeof(data), 0);
}

void Connection::receiveDirectory() {
    std::cout << "Waiting directory from user " << username << std::endl;

    while(true) {
        // Receive file or generic message
        Message m = awaitMessage();
        if (m.getType() == MessageType::text && m.getMessage() == "END") break;

        FileWrapper file = m.getFileWrapper();
        switch (file.getStatus()) {
            case FileStatus::created: {
                char *data = file.getData();
                f->writeFile(file.getPath(), data, sizeof(data));
                break;
            }
            case FileStatus::modified: {
                char *data = file.getData();
                f->writeFile(file.getPath(), data, sizeof(data));
                break;
            }
            case FileStatus::erased: {
                f->deleteFile(file.getPath());
                break;
            }
            default:
                break;
        }

        sendMessage(Message("ACK"));
    }
}

void Connection::sendDirectory() {
    std::cout << "Sending directory to user " << username << std::endl;

    for (const filesystem::path& path : f->getContent()) {
        if(filesystem::is_directory(path)) continue;

        ssize_t size = f->getFileSize(path);
        char* buf = new char[size];

        if(!f->readFile(path, buf, size)) {
            sendMessage(Message("FS_ERR"));
            break;
        }

        FileWrapper file(path, buf, FileStatus::created); // TODO: check if 'created' is the right status
        sendMessage(Message(std::move(file)));
    }

    sendMessage(Message("TERMINATED"));

}

void Connection::receiveFile() {
    Message m = awaitMessage();

    FileWrapper file = m.getFileWrapper();
    switch (file.getStatus()) {
        case FileStatus::created: {
            char *data = file.getData();
            f->writeFile(file.getPath(), data, sizeof(data));
            break;
        }
        case FileStatus::modified: {
            char *data = file.getData();
            f->writeFile(file.getPath(), data, sizeof(data));
            break;
        }
        case FileStatus::erased: {
            f->deleteFile(file.getPath());
            break;
        }
        default:
            break;
    }

    sendMessage(Message("ACK"));
}

Connection::Connection(Connection &&other) {
    socket = std::move(other.socket);
    username = std::move(other.username);
    f = other.f;
    other.f = nullptr;
    logged = other.logged;
    terminate = other.terminate;
    std::cout <<"CONNECTION MOVE 2"<<std::endl;
}

Connection &Connection::operator=(Connection &&other) {
    if (this != &other) {
        socket = std::move(other.socket);
        username = std::move(other.username);
        f = other.f;
        other.f = nullptr;
        logged = other.logged;
        terminate = other.terminate;
        std::cout <<"CONNECTION MOVE"<<std::endl;
    }
    return *this;
}




