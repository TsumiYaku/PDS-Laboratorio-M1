#include "Connection.h"

Connection::Connection(Socket socket): socket(std::move(socket)) {}

Message Connection::awaitMessage() {
    // Socket read
    char buf[1024];
    int size = socket.read(buf, sizeof(buf), 0);

    // Unserialization
    std::stringstream sstream;
    sstream << buf;
    Deserializer ia(sstream);
    Message m(MessageType::text);
    m.unserialize(ia, 0);

    return m;
}

void Connection::run() {
    // Initially awaits login
    Message m = awaitMessage();

    // If it's not a login message...
    // TODO: implement better auth
    if(m.getMessage().find("LOGIN") == std::string::npos) {
        sendResponse("NOT_OK");
        return;
    } else {
        sendResponse("OK");

        // Extract username from login msg
        std::string msg = m.getMessage();
        int first, second;
        first = msg.find("_") + 1;
        second = msg.find("_", first);
        username = msg.substr(first, second-first);
        f = new Folder(username, username);
    }

    // Await further messages
    m = awaitMessage();

    // Client can't simply send files, must first send a message
    if(m.getType() == MessageType::file)
        sendResponse("NOFILE");
    else
        handlePacket(m);
}

void Connection::handlePacket(Message m) {
    std::string msg(m.getMessage());
    std::string response;

    // Check what message has been received
    // TODO: can't we somehow use a switch? Doesn't allow me with std::string
    if(msg == "CHECK") // If client asks checksum
        sendChecksum();
    else if (msg == "SYNC") { // If client asks to sync files
        sendChecksum();
        // TODO
    }

}

void Connection::sendChecksum() {
    int data = (int)f->getChecksum();
    ssize_t size = socket.write(&data, sizeof(data), 0);
}

void Connection::receiveFiles() {
    while(true) {
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

        sendResponse("ACK");
    }
}

void Connection::sendFiles() {
    // TODO
}

void Connection::sendResponse(const std::string& r) {
    socket.write(r.c_str(), sizeof(r.c_str()), 0);
}




