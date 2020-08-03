#include "Client.h"

int main(){
   Client c(0, std::string("127.0.0.1"), 8000);
   while(c.doLogin("user", "pwd")){}
   c.monitoraCartella(".");

   return 0;
}
