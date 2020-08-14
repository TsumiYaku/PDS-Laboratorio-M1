#include "Exchanger.h"

using namespace MessageExchanger;

Message MessageExchanger::awaitMessage(Socket *socket, int msg_size, MessageType type) {
    // Socket read
    std::unique_ptr<char[]> buf = std::make_unique<char[]>(msg_size);
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
    }
    catch (boost::archive::archive_exception& e) {
        throw std::runtime_error(e.what());
    }

    if(m.getType() == MessageType::text)
        std::cout << "Message received: " << m.getMessage() << std::endl;
    else
        std::cout << "Message received" << std::endl;
    return m;
}

void MessageExchanger::sendMessage(Socket* socket, Message &&m) {
    // Serialization
    std::stringstream sstream;
    try {
        Serializer oa(sstream);
        m.serialize(oa, 0);
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
        std::cout << "Message sent: " << m.getMessage() << std::endl;
    else
        std::cout << "Sending file info: " << m.getFileWrapper().getPath().relative_path()<< std::endl;
}

bool FileExchanger::receiveFile(Socket *socket, Folder* f) {
    // Receive command and answer ACK
    Message m = awaitMessage(socket);
    sendMessage(socket, Message("ACK"));

    // In case we're done receiving a series
    if(m.getMessage().compare("END") == 0) return false;

    // Errors handling
    if(m.getMessage().compare("FS_ERR") == 0) throw std::runtime_error("receiveFile: File system error on client side");
    if(m.getMessage().compare("ERR") == 0) throw std::runtime_error("receiveFile: Generic error on client side");

    // Receive file info
    FileWrapper fileInfo = awaitMessage(socket, SIZE_MESSAGE_TEXT, MessageType::file).getFileWrapper();
    filesystem::path file = f->getPath()/fileInfo.getPath();
    if(filesystem::exists(file) && (fileInfo.getStatus() == FileStatus::created))
       sendMessage(socket, Message("PRESENT")); 
    else{
        sendMessage(socket, Message("ACK"));
    
        // Check if the received message was a Directory or a File
        if(m.getMessage().compare("DIRECTORY") == 0 )
            switch (fileInfo.getStatus()) {
                case FileStatus::modified : // If modified it first deletes the folder, then re-creates it (so no break)
                case FileStatus::created :
                    f->writeDirectory(fileInfo.getPath());
                    break;
                case FileStatus::erased :
                    f->deleteFile(fileInfo.getPath());
                    break;
                default:
                    break;
            }
        else if(m.getMessage().compare("FILE") == 0 ) {
            switch (fileInfo.getStatus()) {
                case FileStatus::modified :{
                    f->deleteFile(fileInfo.getPath()); // deletes file in any case just to be sure
                    std::cout << "Reading blocks from socket" << std::endl;
                    
                    // Read chunks of data
                    int count_char = fileInfo.getSize();
                    int num = 0;
                    while (count_char > 0) {
                        std::cout << "Remaining data: " << count_char << std::endl;
                        num = count_char > SIZE_MESSAGE_TEXT ? SIZE_MESSAGE_TEXT : count_char;
                        std::unique_ptr<char[]> buf = std::make_unique<char[]>(num);
                        int receivedSize = socket->read(buf.get(), num, 0);
                        f->writeFile(fileInfo.getPath(), buf.get(), receivedSize);
                        count_char -= receivedSize;
                    }
                    break;
                }
                case FileStatus::created : {
                    // Read chunks of data
                    int count_char = fileInfo.getSize();
                    int num = 0;
                    while (count_char > 0) {
                        std::cout << "Remaining data: " << count_char << std::endl;
                        num = count_char > SIZE_MESSAGE_TEXT ? SIZE_MESSAGE_TEXT : count_char;
                        std::unique_ptr<char[]> buf = std::make_unique<char[]>(num);
                        int receivedSize = socket->read(buf.get(), num, 0);
                        f->writeFile(fileInfo.getPath(), buf.get(), receivedSize);
                        count_char -= receivedSize;
                    }
                    
                    break;
                }
                case FileStatus::erased :
                    f->deleteFile(fileInfo.getPath());
                    break;
                default:
                    break;
            }
        }
        else if (m.getMessage().compare("FILE_DEL") == 0) {
            if(!fileInfo.getPath().empty())
                f->deleteFile(fileInfo.getPath());
        }
    }
    // Send ACK to indicate operation success
    sendMessage(socket, Message("ACK"));
    return true;
}

void FileExchanger::sendFile(Socket *socket, Folder* f, const filesystem::path& path, FileStatus status) {
    filesystem::path filePath = f->getPath()/path;

    // Sending command message
    std::string command;
    int size = 0;
    if(filesystem::is_directory(filePath))
        command = "DIRECTORY";
    else if(filesystem::is_regular_file(filePath)) {
        command = "FILE";
        size = f->getFileSize(path);
    }
    else if(!filesystem::exists(filePath))
        command = "FILE_DEL";
    else { // in case of any error
        sendMessage(socket, Message("ERR"));
        throw std::runtime_error("File not supported");
    }
    sendMessage(socket, Message(std::move(command)));
    Message m = awaitMessage(socket); // waiting for ACK

    // Sending File Info
    FileWrapper fileInfo = FileWrapper(path, status, size);//path, status, size file
    sendMessage(socket, Message(std::move(fileInfo)));
    m = awaitMessage(socket);

    if(m.getMessage().compare("ACK") == 0){

        // if it's a file, also send the data
        if(is_regular_file(filePath) && (command.compare("FILE_DEL") != 0)){
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
                    sendMessage(socket, Message("FS_ERR"));
                    m = awaitMessage(socket);
                    throw std::runtime_error("Impossible read file");
                };

                // Socket sending
                socket->write(buf.get(), num, 0);
                count_char -= num;
            }
            file.close();
        }
    }

    m = awaitMessage(socket); //waiting final ACK
}

