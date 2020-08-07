#include "Client.h"
#define NUM_ERROR_REPEATING 100

//TODO BETTER MANAGMENT ERROR
int main(int argc, char* argv[]){
   if(argc != 6){
      std::cout<<"USAGE ./client ADRESS PORT USER PASSWORD DIRECTORY"<<std::endl;
      exit(-1);
   }

   Client c(std::string(argv[1]), atoi(argv[2]));
   while(!c.doLogin(argv[3], argv[4])){}

   filesystem::path dir(argv[5]);
   if(!exists(dir)){
      std::cout << "DIRECTORY NOT EXIST" << std::endl;
      exit(-2);
   }

   int cont_error = 0;
   while(true){
      try{
         c.monitoraCartella(argv[5]);
      }catch(std::runtime_error& e){
         std::cout << e.what() << std::endl;
         std::cout << "Riprovo tra 1 sec..." << std::endl;
         std::this_thread::sleep_for(std::chrono::milliseconds(1000));
         cont_error++;
         if(cont_error == NUM_ERROR_REPEATING)
           break;
      }
      catch(boost::filesystem::filesystem_error& e){
         std::cout << e.what() << std::endl;
         std::cout << "Riprovo tra 1 sec..." << std::endl;
         std::this_thread::sleep_for(std::chrono::milliseconds(1000));
         cont_error++;
         if(cont_error == NUM_ERROR_REPEATING)
           break;
      }
   }

   return 0;
}
