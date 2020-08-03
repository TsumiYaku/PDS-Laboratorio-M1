#include "Message.h"

Message::Message(MessageType type):type(type){}

Message::Message(std::string message):message(message){
    type = MessageType::text;
    file = FileWrapper();
}

Message::Message(FileWrapper file): file(file){
    type = MessageType::file;
    message = std::string("fileWrapper");
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

FileWrapper Message::getFileWrapper(){
    if(type == MessageType::file)
       return file;
    return FileWrapper();
}

std::string Message::getMessage(){
    return message;
}

MessageType Message::getType() {
    return type;
}
