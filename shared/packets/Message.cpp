#include "Message.h"

Message::Message(MessageType type):type(type){}

Message::Message(std::string message):message(message){
    type = MessageType::text;
    file = FileWrapper();
}

Message::Message(FileWrapper file): file(std::move(file)){
    type = MessageType::file;
    message = std::string("fileWrapper");
}

Message::Message(Message &&other) {
    message = std::move(other.message);
    file = std::move(other.file);
    type = other.type;
}

Message &Message::operator=(Message &&other) noexcept {
    if(this != &other) {
        message = std::move(other.message);
        file = std::move(other.file);
        type = other.type;
    }

    return *this;
}
    
void Message::print(){
    switch(type){
         case MessageType::text: 
         {
            std::cout << "MESSAGGIO:" << message <<std::endl;
            break;
         } 
         case MessageType::file:
         {
            std::cout << "FILE:" << message <<std::endl;
            file.print();
            break;
         } 
    }
}

FileWrapper&& Message::getFileWrapper(){
    if(type == MessageType::file)
       return std::move(file);
    return FileWrapper();
}

std::string Message::getMessage(){
    return message;
}

MessageType Message::getType() {
    return type;
}
