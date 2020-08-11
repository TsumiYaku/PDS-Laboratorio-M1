#pragma once
#include <iostream> 
#include <string>

class SimpleCrypt{    
       char key[7] = {'A', 'W', 'C', 'K', 'L', 'Q', 'M'}; //Any chars will work
       
       public:
       SimpleCrypt(){}
       std::string encrypt(std::string);
       std::string decrypt(std::string);
       ~SimpleCrypt(){} 
};


