#pragma once

#include <boost/filesystem.hpp>
#include <boost/serialization/serialization.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/serialization/access.hpp>
#include <string>
#include <string.h>
#include <iostream>
#include <packets/FileStatus.h>

using namespace boost;
using namespace serialization;

class FileWrapper {
    friend class boost::serialization::access;
    
    filesystem::path filePath;
    //char* data;
    int len;
    FileStatus status;
    
public:
    FileWrapper();
    FileWrapper(filesystem::path filePath, FileStatus status, int len);
    FileWrapper(FileWrapper&&);
    FileWrapper& operator=(FileWrapper&&) noexcept ;

    FileWrapper(const FileWrapper&) = delete;
    FileWrapper& operator=(const FileWrapper&) = delete;

    void print();
    filesystem::path getPath();
    //char* getData();
    FileStatus getStatus();
    void setSize(int size);
    int getSize();
    
    template<class Archive>
    void serialize(Archive& ar, const unsigned int version){
        ar << BOOST_SERIALIZATION_NVP(filePath.relative_path().string());
        switch(status){
            case FileStatus::created: ar << 0; break;
            case FileStatus::modified: ar << 1; break;
            case FileStatus::erased: ar << 2; break;
        }
        ar << len;
        //ar & serialization::make_array<char>(data, len);
    }

    template<class Archive>
    void unserialize(Archive& ar, const unsigned int version){
        int st;
        std::string p;
        ar >>  BOOST_SERIALIZATION_NVP(p); 
        filePath = filesystem::path(p);
        ar >> st;
        switch(st){
            case 0: status = FileStatus::created; break;
            case 1: status = FileStatus::modified; break;
            case 2: status = FileStatus::erased; break;
        }
        ar >> len;
        //data = new char[len];
        //ar & serialization::make_array<char>(data, len);
    }
};
