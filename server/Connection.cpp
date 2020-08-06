#include "Connection.h"

Connection::Connection(Socket&& socket) {
     this->socket = std::move(socket);
}

Message Connection::awaitMessage(size_t msg_size = 1024) {
    // Socket read
    char buf[msg_size];
    int size = socket.read(buf, msg_size, 0);
    
    //std::cout << buf << " " << size << std::endl;
    // Unserialization
    std::stringstream sstream;
    sstream << buf;
    Deserializer ia(sstream);
    Message m(MessageType::text);
    m.unserialize(ia, 0);

    if (logged)
        std::cout << "Received message from user: " << username << std::endl;
   
    if(m.getType() == MessageType::text)
        std::cout << "Message received: " << m.getMessage() << std::endl;
    else
        std::cout << "Incoming file..." << m.getFileWrapper().getPath().relative_path()<< std::endl;
    return m;
}

void Connection::sendMessage(Message &&m) {
   
    //m.print();
    // Serialization

    std::stringstream sstream;
    Serializer oa(sstream);
    m.serialize(oa, 0);

    // Socket write
    std::string s(sstream.str());
    socket.write(s.c_str(), strlen(s.c_str())+1, 0);

    if (logged)
        std::cout << "Sending message to user: " << username << std::endl;
    if(m.getType() == MessageType::text)
        std::cout << "Message sent: " << m.getMessage() << std::endl;
    else
        std::cout << "Sending file..." << m.getFileWrapper().getPath().relative_path()<< std::endl;
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
        // Extract username from login msg
        std::string msg = m.getMessage();
        int first, second;
        first = msg.find("_") + 1;
        second = msg.find("_", first);
        username = msg.substr(first, second-first);
        logged = true;
        f = new Folder(username, username);

        std::cout << "User " << username << " successfully authenticated" << std::endl;
        sendMessage(Message("OK"));
    }

    listenPackets();
}

void Connection::listenPackets() {
    while(true) {
        if (terminate || !logged)
            return;

         //Await further messages
        Message m = awaitMessage();

         //Client can't simply send files, must first send a message
        if(m.getType() == MessageType::file)
            sendMessage(Message("ERR"));
        else
            handlePacket(std::move(m));
    }
}

void Connection::handlePacket(Message &&m) {
    std::string msg(m.getMessage());
    std::string response;
    //std::cout << msg << std::endl;

    // Check what message has been received
    // TODO: can't we somehow use a switch? Doesn't allow me with std::string
    if(msg == "CHECK") // If client asks checksum
        sendChecksum();
    else if (msg == "SYNC") // If client asks to sync files
        synchronize();
    else if (msg == "CREATE" || msg == "MODIFY" || msg == "ERASE") { // Folder listening management
        sendMessage(Message("ACK"));
        while(receiveFile()) {};
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
        remove_all(f->getPath());
        receiveDirectory();
    }
    else if(msg == "DOWNLOAD") {
        sendMessage(Message("ACK"));
        sendDirectory();
    }
}

void Connection::sendChecksum() {
    int data = (int)f->getChecksum();
    std::cout << "SEND CHECKSUM: " << data << std::endl;
    ssize_t size = socket.write(&data, sizeof(data), 0);
}

void Connection::receiveDirectory() {
    std::cout << "Waiting directory from user " << username << std::endl;
    char buf[1024];
    std::stringstream ss;
    while(receiveFile()) {}
}

void Connection::sendDirectory() {
    std::cout << "Sending directory to user " << username << std::endl;

    for (const filesystem::path& path : f->getContent()) {

        if(filesystem::is_directory(path)) {
            sendMessage(Message("DIRECTORY"));
            Message m = awaitMessage();
            //std::cout << f->strip_root(path).string() << std::endl;
            FileWrapper file = FileWrapper(f->strip_root(path), strdup(""), FileStatus::created); // TODO: check if 'created' is the right status
            Message m2 = Message(std::move(file));
            sendMessage(std::move(m2));
            std::cout << "DIRECTORY SEND " << m2.getFileWrapper().getPath() << std::endl;
        }else{
            ssize_t size = f->getFileSize(path);
            //std::cout << size <<std::endl;
            char* buf = new char[size];

            if(!f->readFile(path, buf, size)) {
                sendMessage(Message("FS_ERR"));
                break;
            }
            std::cout <<"CONTENT FILE " << buf << std::endl;
            sendMessage(Message("FILE"));
            Message m = awaitMessage(); //attendo un response da client

            FileWrapper file = FileWrapper(f->strip_root(path), buf, FileStatus::created); // TODO: check if 'created' is the right status
            Message m2 = Message(std::move(file));
            //m2.print();
            sendMessage(std::move(m2));
            
            m = awaitMessage();   //attendo response (TODO BETTER)

        }
    }

    sendMessage(Message("END"));

}

//toglie la directory user dal path


bool Connection::receiveFile() {
        std::stringstream ss;
        Message m = awaitMessage();
        sendMessage(Message("OK"));
        char buf[1024];
        if(m.getMessage().compare("END") == 0) return false;
        if(m.getMessage().compare("FS_ERR") == 0) return false;
        if(m.getMessage().compare("ERR") == 0) return false;
        
        //recieve message FILE and after recieve filewrapper
        int size = socket.read(buf, sizeof(buf), 0);
        ss << buf;
        
        if(m.getMessage().compare("DIRECTORY") == 0 )
        {
            Deserializer ia(ss);
            Message m = Message(MessageType::file);
            m.unserialize(ia, 0);
            FileWrapper file = m.getFileWrapper();
            std::cout << " DIRECTORY " << file.getPath() <<std::endl;
            switch (file.getStatus()) {
                case FileStatus::created: {
                    f->writeDirectory(file.getPath().relative_path());
                    break;
                }
                case FileStatus::modified: {
                    f->writeDirectory(file.getPath().relative_path());
                    break;
                }
                case FileStatus::erased: {
                    //if(exists(file.getPath().relative_path()))
                    //{
                        filesystem::path filePath;
                        if(file.getPath().string().substr(0,2) == "./"){
                           filePath = f->getPath()/file.getPath().string().substr(2, file.getPath().string().length() - 2);
                        }
                        else
                            filePath = file.getPath().relative_path();

                        f->deleteFile(filePath);
                    //}
                    break;
                }
                default:
                    break;
            }
        }
        else{
            Deserializer ia(ss);
            Message m = Message(MessageType::file);
            m.unserialize(ia, 0);
            FileWrapper file = m.getFileWrapper();
            std::cout << " FILE " << file.getPath() <<std::endl;
            switch (file.getStatus()) {
                case FileStatus::created: {
                    char *data = file.getData();
                    f->writeFile(file.getPath().relative_path(), data, strlen(data));
                    break;
                }
                case FileStatus::modified: {
                    char *data = file.getData();
                    f->writeFile(file.getPath().relative_path(), data, strlen(data));
                    break;
                }
                case FileStatus::erased: {
                    filesystem::path filePath;
                    if(file.getPath().string().substr(0,2) == "./"){
                        filePath = f->getPath()/file.getPath().string().substr(2, file.getPath().string().length() - 2);
                    }
                    else
                        filePath = file.getPath().relative_path();
                    std::cout << "DELETE "<< filePath << std::endl;

                    f->deleteFile(filePath);
                    break;
                }
                default:
                    break;
            }
        }

        sendMessage(Message("ACK"));
        return true;
}

Connection::Connection(Connection &&other) {
    socket = std::move(other.socket);
    username = std::move(other.username);
    f = other.f;
    other.f = nullptr;
    logged = other.logged;
    terminate = other.terminate;
    std::cout <<"CONNECTION MOVE"<<std::endl;
}

Connection &Connection::operator=(Connection &&other) {
    if (this != &other) {
        socket = std::move(other.socket);
        username = std::move(other.username);
        f = other.f;
        other.f = nullptr;
        logged = other.logged;
        terminate = other.terminate;
        std::cout <<"CONNECTION MOVE OPERATOR="<<std::endl;
    }
    return *this;
}




