#include "Client.h"

int main(int argc, char* argv[]){
   if(argc != 3){
      std::cout<<"USAGE ./client ADRESS PORT"<<std::endl;
      exit(-1);
   }
   try{
      Client c(std::string(argv[1]), atoi(argv[2]));
      while(!c.doLogin("tester", "pwd")){}
      c.monitoraCartella("./prova");
   }catch(std::runtime_error& e){
      std::cout << e.what() << std::endl;
   }
   catch(boost::filesystem::filesystem_error& e){
      std::cout << e.what() << std::endl;
   }

   return 0;
}
