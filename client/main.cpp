#include "Client.h"

int main(int argc, char* argv[]){
   if(argc != 6){
      std::cout<<"USAGE ./client ADRESS PORT USERNAME DIRECTORY"<<std::endl;
      exit(-1);
   }

   Client c(std::string(argv[1]), atoi(argv[2]));
   if(!c.doLogin(argv[3])){
      std::cout << "USERNAME USED TO ONOTHER..." <<std::endl;
   }

   filesystem::path dir(argv[5]);

   if(!exists(dir)){
      std::cout << "DIRECTORY NOT EXIST" << std::endl;
      exit(-2);
   }

   c.monitoraCartella(argv[5]);
      
   return 0;
}
