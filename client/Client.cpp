#include "Client.h"

std::mutex mout;
void log(std::string msg){
    std::lock_guard<std::mutex> lg(mout);
    std::cout<<msg<<std::endl;
}

/*************************CLIENT********************************************/
Client::Client(std::string address, int port): address(address), port(port){
        cont_error = 0;
        struct sockaddr_in sockaddrIn;
        sockaddrIn.sin_port = ntohs(port);
        sockaddrIn.sin_family = AF_INET;
        if(::inet_pton(AF_INET, address.c_str(), &sockaddrIn.sin_addr) <=0)
            throw std::runtime_error("error inet_ptn");
        
        sock.connect(&sockaddrIn, sizeof(sockaddrIn));
}

Client::Client(Client &&other) {

    this->sock = std::move(sock);
    this->cont_error = other.cont_error;
    this->sad = other.sad;
    this->directory = other.directory;
    this->address = std::move(other.address);
    this->port = other.port;
    other.directory = nullptr;
    other.sock.closeSocket();
    
    std::cout <<"CLIENT MOVE"<<std::endl;
}

Client &Client::operator=(Client &&other) {
    if(this->user != other.user){
        this->sock = std::move(sock);
        this->cont_error = other.cont_error;
        this->sad = other.sad;
        this->directory = other.directory;
        this->address = std::move(other.address);
        this->port = other.port;
        other.directory = nullptr;
        other.sock.closeSocket();
        std::cout <<"CLIENT MOVE OPERETOR="<<std::endl;
    }
    return *this;
}

void Client::sendMessage(Message &&m) {
    // Serialization
    std::stringstream sstream;
    Serializer oa(sstream);
    m.serialize(oa, 0);

    // Socket write
    std::string s(sstream.str());
    log("Message send:" + m.getMessage());
    sock.write(s.c_str(), strlen(s.c_str())+1, 0);
}

Message Client::awaitMessage(size_t msg_size = SIZE_MESSAGE_TEXT) {
    // Socket read
    char buf[msg_size];
    int size = sock.read(buf, msg_size, 0);
    //std::cout << buf << " " << size << std::endl;
    // Unserialization
    std::stringstream sstream;
    sstream << buf;
    Deserializer ia(sstream);
    Message m(MessageType::text);
    m.unserialize(ia, 0);
    log("Message recieve:" + m.getMessage());
    return m;
}

void Client::sendMessageWithResponse(std::string message, std::string response) {
    while(true){
         Message m = Message(message);
         sendMessage(std::move(m));
         //read response
         m = awaitMessage();
        if(m.getMessage().compare(response) == 0) break;
    }
}

void Client::close() /*throw(std::runtime_error)*/{
    sock.closeSocket();     
}

Client::~Client(){
    std::cout <<"Client diconnetted..." << std::endl;
    close();
}

bool Client::doLogin(std::string user){
    
    Message m = Message(MessageType::text);
    while(true){
        m = Message("LOGIN_" + user);
        sendMessage(std::move(m));

        //read response
        m = awaitMessage();

        if(m.getMessage().compare("OK") == 0 || m.getMessage().compare("NOT_OK") == 0) break;
    }

    if(m.getMessage().compare("OK") == 0){
        this->user = user; //login effettuato con successo
        return true;
    }
    return false; //login non effettuato
}


void Client::monitoraCartella(std::string folder){
        
        while(true){ //ritento sincornizzazione iniziale in casi di errori per NUM.. volte, altrimenti confuto un errore permanente e chiudo il programma
            try{
                directory = new Folder(user, folder);
            
                path dir(folder);

                std::cout <<"MONITORING " << dir.string() << std::endl;

                Message m = Message("SYNC");
                sendMessage(std::move(m));

                int checksumServer = 0;
                int checksumClient = (int)directory->getChecksum();
                
                sock.read(&checksumServer, sizeof(checksumServer), 0); //ricevo checksum da server
                std::cout << "CHECKSUM CLIENT " << checksumClient <<std::endl;
                std::cout << "CHECKSUM SERVER " << checksumServer <<std::endl;

                if(exists(dir) && checksumClient!=0){ //la cartella esiste e quindi la invio al server  
                    if((checksumServer == 0 && checksumClient != 0) || (checksumClient != checksumServer)){//invio tutta la directory per sincronizzare
                        //invio richiesta update directory server (fino ad ottenere response ACK)
                        sendMessageWithResponse("UPDATE", "ACK");
                        //invio solo i file con checksum diverso
                        for(filesystem::path path: directory->getContent()){ 
                          path = directory->getPath()/path;
                          std::cout << "SENDING: " << path << std::endl;
                          inviaFile(path, FileStatus::modified);
                        }

                        sendMessageWithResponse("END", "OK");
                        /*while(true){
                            // TODO: END is sent before the threads can finish sending all files!!!!
                            Message m = Message("END");
                            sendMessage(std::move(m));
                            m = awaitMessage();
                        if(m.getMessage().compare("OK") == 0) break;
                        }*/     
                    }
                }
                else if(checksumServer != 0 && checksumClient==0){ 
                    sendMessageWithResponse("DOWNLOAD", "ACK");
                    downloadDirectory(); //scarico contenuto del server
                }else{
                    Message m = Message("OK");
                    sendMessage(std::move(m));
                }
                break; //ho eseguito tutto correttamente
            }catch(std::runtime_error& e){
                    std::cout << e.what() << std::endl;
                    std::cout << "TRY TO RESOLVE..." << std::endl;
                    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                    cont_error++;
                    if(cont_error == NUM_POSSIBLE_TRY_RESOLVE_ERROR)
                        exit(-1);
                }
                catch(boost::filesystem::filesystem_error& e){
                    std::cout << e.what() << std::endl;
                    std::cout << "TRY TO RESOLVE..." << std::endl;
                    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                    cont_error++;
                    if(cont_error == NUM_POSSIBLE_TRY_RESOLVE_ERROR)
                        exit(-1);
                }
            };

            //mi metto in ascolto e attendo una modifica
            FileWatcher fw{folder, std::chrono::milliseconds(1000)};
            fw.start([this](std::string path_to_watch, FileStatus status) -> void {
                try{
                    switch(status) {
                        case FileStatus::created:{
                            std::cout << "Created: " << path_to_watch << '\n';
                            //invio richiesta creazione file
                            sendMessageWithResponse("CREATE", "ACK");

                            //invio file
                            inviaFile(path(path_to_watch), FileStatus::created); 

                            sendMessageWithResponse("END", "OK");
                            break;
                        }
                        case FileStatus::modified:{
                            //dico al server che è stato modificato un file e lo invio al server
                            std::cout << "Modified: " << path_to_watch << '\n';

                            //invio richiesta modifica
                            sendMessageWithResponse("MODIFY", "ACK");           

                            //invio il file
                            inviaFile(path(path_to_watch), FileStatus::modified);
                            
                            sendMessageWithResponse("END", "OK");
                            break;
                        }
                        case FileStatus::erased:{
                            //dico al server che è stato modificato un file e lo invio al server
                            std::cout << "Erased: " << path_to_watch << '\n';
                            //invio richiesta cancellazione
                            sendMessageWithResponse("ERASE", "ACK");
                            //invio file
                            inviaFile(path(path_to_watch), FileStatus::erased); 

                            sendMessageWithResponse("END", "OK");
                        }
                        case FileStatus::nothing:{
                            break;
                        }
                        default:break;
                    }
                }catch(std::runtime_error& e){
                    std::cout << e.what() << std::endl;
                    std::cout << "TRY TO RESOLVE..." << std::endl;
                    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                    cont_error++;
                    if(cont_error == NUM_POSSIBLE_TRY_RESOLVE_ERROR)
                        exit(-2);
                }
                catch(boost::filesystem::filesystem_error& e){
                    std::cout << e.what() << std::endl;
                    std::cout << "TRY TO RESOLVE..." << std::endl;
                    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                    cont_error++;
                    if(cont_error == NUM_POSSIBLE_TRY_RESOLVE_ERROR)
                        exit(-2);
                }
        }); 
}


void Client::downloadDirectory(){
    //std::thread download([this]() -> void{
        //std::lock_guard<std::mutex> lg(this->mu);
        std::stringstream ss;
        char* buf = new char[SIZE_MESSAGE_TEXT];
        while(true){
            //read FILE , DIRECTORY, TERMINATED or FS_ERR
            Message m = awaitMessage();
            sendMessage(Message("OK"));
            if(m.getMessage().compare("END") == 0) break;
            if(m.getMessage().compare("FS_ERR") == 0) break;

            if(m.getMessage().compare("DIRECTORY") == 0){
                sock.read(buf, SIZE_MESSAGE_TEXT, 0);
                ss << buf;
                // Unserialization
                Deserializer ia(ss);
                m = Message(MessageType::file);
                m.unserialize(ia, 0);
                FileWrapper f = m.getFileWrapper();
                directory->writeDirectory(f.getPath());
            }
            else{
                int size_text_serialize = 0;
                sock.read(&(size_text_serialize), sizeof(size_text_serialize), 0);
                buf = new char[size_text_serialize];
                sendMessage(Message("ACK"));
                sock.read(buf, size_text_serialize, 0);
                ss << buf;
                // Unserialization
                Deserializer ia(ss);
                m = Message(MessageType::file);
                m.unserialize(ia, 0);
                FileWrapper f = m.getFileWrapper();
                
                char * data = strdup(f.getData());
                directory->writeFile(f.getPath(), data, strlen(data));
            }
            m = Message("ACK");
            sendMessage(std::move(m));
        };
    //});
    //download.detach();
}

void Client::inviaFile(filesystem::path path_to_watch, FileStatus status) /*throw(filesystem::filesystem_error, std::runtime_error)*/{
    std::thread synch([this, path_to_watch, status] () -> void {
        std::lock_guard<std::mutex> lg(this->mu);

        //path p(path_to_watch);
        std::stringstream ss;
        while(true){
            std::cout << "READ PATH: "<< path_to_watch.c_str() << std::endl;
            if(is_directory(path_to_watch)){
                sendMessage(Message("DIRECTORY"));
                Message m = awaitMessage(); //attendo un response da server
                //invio directory da sincronizzare

                FileWrapper f = FileWrapper(directory->removeFolderPath(path_to_watch.string()), strdup(""), status);
               
                Message m2 = Message(std::move(f));
                //m2.print();
                sendMessage(std::move(m2));

                std::cout << "DIRECTORY SEND " << directory->removeFolderPath(path_to_watch.string()) << std::endl;
                
                //ricevo response
                m = awaitMessage();

                //leggo messaggio da server
                if(m.getMessage().compare("ACK") == 0) break;
            }else if(is_regular_file(path_to_watch)){
                //invio file da sincronizzare
                filesystem::path relativeContent = path(directory->removeFolderPath(path_to_watch.string()));
                
                int size = directory->getFileSize(relativeContent);
                char* buf = new char[size];
                
                if(!directory->readFile(relativeContent, buf, size)){
                    sendMessage(Message("FS_ERR"));
                    Message m = awaitMessage(); 
                    throw std::runtime_error("Error read file");
                }

                sendMessage(Message("FILE"));
                Message m = awaitMessage(); 

                FileWrapper f = FileWrapper(relativeContent, buf, status);
                
                Message m2 = Message(std::move(f));
                Serializer ia(ss);
                m2.serialize(ia, 0);
                std::string s(ss.str());
                size = s.length() + 1;
                this->sock.write(&size, sizeof(size), 0);//invio taglia testo da deserializzare
                m = awaitMessage(); //attendo ACK
                this->sock.write(s.c_str(), strlen(s.c_str())+1, 0);//invio testo serializzato
                //sendMessage(std::move(m2));
                
                std::cout << "FILE SEND " << relativeContent << std::endl;
                
                //ricevo response
                m = awaitMessage();  

                //leggo ACK
                if(m.getMessage().compare("ACK") == 0) break;
            }else if(!exists(path_to_watch)){ //file cancellato
                Message m = Message("FILE_DEL");
                sendMessage(std::move(m));
                filesystem::path relativeContent = path(directory->removeFolderPath(path_to_watch.string()));

                FileWrapper f = FileWrapper(relativeContent , strdup(""), status);
               
                Message m2 = Message(std::move(f));
                //m2.print();
                sendMessage(std::move(m2));

                std::cout << "FILE DELETED " << relativeContent  << std::endl;

                 m = awaitMessage();  

                //leggo ACK
                if(m.getMessage().compare("ACK") == 0) break;
                break;
            }else{
                Message m = Message("ERR");
                sendMessage(std::move(m));
                throw std::runtime_error("File not supported");
            }
        }
        
    });
    synch.detach();
}
