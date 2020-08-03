#pragma once
#include <iostream>
#include "FileWrapper.h"

using Serializer = boost::archive::text_oarchive;
using Deserializer = boost::archive::text_iarchive;

enum class MessageType {text, file};

class Message {

    std::string message; //message
    FileWrapper file;
    MessageType type;

public:
    Message();
    Message(std::string message);
    Message(FileWrapper file);
    
    void print();

    FileWrapper getFileWrapper();
    std::string getMessage();

    template<class Archive>
    void serialize(Archive& ar, const unsigned int version){
        if(type == MessageType::file){
            ar & 1; //type == file

            std::stringstream s;
            Serializer oa(s);
            file.serialize(oa, 0);
            std::string tmp = s.str();
            ar & BOOST_SERIALIZATION_NVP(tmp);
        }
        else{
            ar & 0; //type == text
            ar & BOOST_SERIALIZATION_NVP(message);
        }
    }

    template<class Archive>
    void unserialize(Archive& ar, const unsigned int version){
            int t;
            ar & t;
            switch(t){
                case 0:{
                    ar & BOOST_SERIALIZATION_NVP(message);
                    type = MessageType::text; break;
                } 
                case 1: {
                    std::string fileSerialize;
                    std::stringstream s;
                    ar & BOOST_SERIALIZATION_NVP(fileSerialize);
                    s << fileSerialize;
                    Deserializer ia(s);
                    file.unserialize(ia, 0);
                    type =  MessageType::file; break;
                } 
            }
    }
    
    
};


