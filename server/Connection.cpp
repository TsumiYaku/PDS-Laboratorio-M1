#include "Connection.h"
std::mutex Connection::dbLock;
std::list<std::string> Connection::users;

Connection::Connection(Socket&& socket) {
     this->socket = std::move(socket);
}

Connection::~Connection() {
    std::lock_guard<std::mutex> lg(dbLock);
    users.remove(username);
}

Message Connection::awaitMessage(size_t msg_size = SIZE_MESSAGE_TEXT) {
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

        Connection::dbLock.lock();
        if (std::find(users.begin(), users.end(), username) != users.end()) {
            std::cout << "User already logged" << std::endl;
            sendMessage(Message("NOT_OK"));
            return;
        }
        else
            users.push_back(username);


        Connection::dbLock.unlock();

        std::cout << "User " << username << " successfully authenticated" << std::endl;
        sendMessage(Message("OK"));
    }

    try { // TODO: TEMPORARY WORKAROUND, MUST implement better error management
        listenPackets();
    }
    catch (...) {
        return;
    }
}

void Connection::listenPackets() {
    while(!terminate && logged) {
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
        f->wipeFolder();
        downloadDirectory();
    }
    else if(msg == "DOWNLOAD") {
        sendMessage(Message("ACK"));
        uploadDirectory();
    }
}

void Connection::sendChecksum() {
    int data = (int)f->getChecksum();
    //std::cout << "SEND CHECKSUM: " << data << std::endl;
    ssize_t size = socket.write(&data, sizeof(data), 0);
}

void Connection::downloadDirectory() {
    std::cout << "Waiting directory from user " << username << std::endl;
    char buf[SIZE_MESSAGE_TEXT];
    std::stringstream ss;
    while(receiveFile()) {}
}

void Connection::uploadDirectory() {
    std::cout << "Sending directory to user " << username << std::endl;

    for (const filesystem::path& path : f->getContent()) {

        if(filesystem::is_directory(path)) {
            sendMessage(Message("DIRECTORY"));
            Message m = awaitMessage();
            //std::cout << f->strip_root(path).string() << std::endl;
            FileWrapper file = FileWrapper(f->strip_root(path), strdup(""), FileStatus::created); // TODO: check if 'created' is the right status
            Message m2 = Message(std::move(file));
            std::cout << "DIRECTORY SEND " << m2.getFileWrapper().getPath() << std::endl;
            sendMessage(std::move(m2));
        }else{
            int size = (int)f->getFileSize(f->removeFolderPath(path.string()));
            //std::cout << size <<std::endl;
            char* buf = new char[size+1];

            if(!f->readFile(f->removeFolderPath(path.string()), buf, size)) {
                sendMessage(Message("FS_ERR"));
                break;
            }
            std::cout <<"CONTENT FILE " << buf << std::endl;

            sendMessage(Message("FILE"));
            Message m = awaitMessage(); //attendo un response da client

        
            FileWrapper file = FileWrapper(filesystem::path(f->removeFolderPath(path.string())), buf, FileStatus::created); // TODO: check if 'created' is the right status

            Message m2 = Message(std::move(file)); //invio file
            std::stringstream ss;
            Serializer ia(ss);
            m2.serialize(ia, 0);
            std::string s(ss.str());
            size = s.length() + 1;
            socket.write(&(size), sizeof(size), 0);//invio taglia testo da deserializzare
            m = awaitMessage(); //attendo ACK
            socket.write(s.c_str(), strlen(s.c_str())+1, 0);//invio testo serializzato
            //m2.print();
            //sendMessage(std::move(m2));

            m = awaitMessage();   //attendo response (TODO BETTER)

        }
    }

    sendMessage(Message("END"));

}

bool Connection::receiveFile() {
        std::stringstream ss;
        Message m = awaitMessage();
        sendMessage(Message("OK"));
        
        if(m.getMessage().compare("END") == 0) return false;
        if(m.getMessage().compare("FS_ERR") == 0) return false;
        if(m.getMessage().compare("ERR") == 0) return false;

        char* buf  = new char[SIZE_MESSAGE_TEXT];
        //recieve message FILE and after recieve filewrapper
    
        if(m.getMessage().compare("DIRECTORY") == 0 )
        {
            socket.read(buf, SIZE_MESSAGE_TEXT, 0);
            ss << buf;
            Deserializer ia(ss);
            Message m = Message(MessageType::file);
            m.unserialize(ia, 0);

            FileWrapper file = m.getFileWrapper();
            std::cout << "Receiving directory: " << file.getPath() <<std::endl;
            switch (file.getStatus()) {
                case FileStatus::created: {
                    f->writeDirectory(file.getPath());
                    break;
                }
                case FileStatus::modified: {
                    f->writeDirectory(file.getPath());
                    break;
                }
                case FileStatus::erased: {
                    f->deleteFile(file.getPath());
                    break;
                }
                default:
                    break;
            }
        }
        else if(m.getMessage().compare("FILE") == 0 ){
            int size_text_serialize = 0;
            socket.read(&(size_text_serialize), sizeof(size_text_serialize), 0);
            sendMessage(Message("ACK"));
            buf = new char[size_text_serialize];
            socket.read(buf, size_text_serialize, 0);
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
                    f->writeFile(file.getPath(), data, strlen(data));
                    break;
                }
                case FileStatus::modified: {
                    char *data = strdup(file.getData());
                    f->writeFile(file.getPath(), data, strlen(data));
                    break;
                }
                case FileStatus::erased: {
                    f->deleteFile(file.getPath());
                    break;
                }
                default:
                    break;
            }
        }
        else{//FILE_DEL
            buf = new char[SIZE_MESSAGE_TEXT];
            socket.read(buf, SIZE_MESSAGE_TEXT, 0);
            ss << buf;
            Deserializer ia(ss);
            Message m = Message(MessageType::file);
            m.unserialize(ia, 0);
            FileWrapper file = m.getFileWrapper();
            std::cout << " FILE DELETED " << file.getPath() <<std::endl;
            if(file.getPath().string().compare("") != 0)
                f->deleteFile(file.getPath());
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
