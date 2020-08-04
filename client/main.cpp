#include "Client.h"

int main(){
   try{
      Client c(0, std::string("127.0.0.1"), 8000);
      while(c.doLogin("user", "pwd")){}
      c.monitoraCartella(".");
   }catch(std::runtime_error& e){
      std::cout << e.what() << std::endl;
   }
   catch(boost::filesystem::filesystem_error& e){
      std::cout << e.what() << std::endl;
   }

   return 0;
}
