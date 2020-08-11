#include "Client.h"

int main(int argc, char* argv[]){
   if(argc != 6){
      std::cout<<"USAGE ./client ADRESS PORT USERNAME PASSWORD DIRECTORY"<<std::endl;
      exit(-1);
   }
   int cont_error=0;
   filesystem::path dir(argv[5]);

   if(!exists(dir)){
      std::cout << "DIRECTORY NOT EXIST" << std::endl;
      exit(-2);
   }

   //while(true){
   Client c(Client(std::string(argv[1]), atoi(argv[2])));
   if(!c.doLogin(argv[3], argv[4])){
       std::cout << "USERNAME USED TO ONOTHER..." <<std::endl;
   }
   c.monitoraCartella(argv[5]);
         //break;
      
   //}

  

   
      
   return 0;
}
