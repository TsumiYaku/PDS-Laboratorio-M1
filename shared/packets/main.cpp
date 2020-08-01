
#include <boost/serialization/access.hpp>
#include <boost/serialization/string.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include "FileWrapper.h"
#include <fstream>

using namespace boost::filesystem;
using Serializer = boost::archive::text_oarchive;
using Deserializer = boost::archive::text_iarchive;


//per compilazione  "g++ FileWrapper.cpp main.cpp -lboost_serialization -lboost_system  -lboost_filesystem -o main" ""./main"
int main()
{
    //serialization

        char data[] = {'c', 'i', 'a', 'o'};
        FileWrapper f = FileWrapper(path("./prova"), data, FileStatus::created);

        std::ofstream ofs("tmp.txt");
        Serializer oa(ofs);
        f.serialize(oa, 0);
        
        f.print();
        ofs.close();

        //deserialization
        FileWrapper clone = FileWrapper();
        std::ifstream ifs("tmp.txt");
        Deserializer ia(ifs);
        clone.deserialize(ia, 0);
        ifs.close();
        clone.print();
    
}
