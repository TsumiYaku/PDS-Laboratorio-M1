
#include <boost/serialization/access.hpp>
#include <boost/serialization/string.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include "FileWrapper.h"
#include "Message.h"
#include <fstream>

using namespace boost::filesystem;
using Serializer = boost::archive::text_oarchive;
using Deserializer = boost::archive::text_iarchive;


//per compilazione  "g++ FileWrapper.cpp Message.cpp main.cpp -lboost_serialization -lboost_system  -lboost_filesystem -o main" ""./main"
int main()
{
        /******************FILE*******************/
        std::cout<<"FILE MESSAGE SERIALIZE/UNSERIALIZE"<<std::endl;
        char data[] = {'c', 'i', 'a', 'o'};
        FileWrapper f = FileWrapper(path("./prova"), data, FileStatus::created);
        Message m = Message(f);
        m.print();

        //serialize
        std::stringstream ss;
        Serializer oa(ss);
        m.serialize(oa, 0);
        std::cout<<"\nContenuto archivio testuale (da inviare a server/client):\n"<< ss.str()<<std::endl << std::endl;

        //deserialization
        Deserializer ia(ss);
        m.unserialize(ia, 0);
        m.print();
        
        /******************MESSAGE*******************/
        std::cout<<"\n\n\nTEXT MESSAGE SERIALIZE/UNSERIALIZE"<<std::endl;
        Message m2 = Message(std::string("ACK"));
        m2.print();

        //serialize
        std::stringstream ss2;
        Serializer oa2(ss2);
        m2.serialize(oa2, 0);
        std::cout<<"\nContenuto archivio testuale (da inviare a server/client):\n"<< ss2.str()<<std::endl<< std::endl;

        //deserialization
        Deserializer ia2(ss2);
        m2.unserialize(ia2, 0);
        m2.print();
}
