#include "Client.h"

std::mutex mout;
void log(std::string msg){
    std::lock_guard<std::mutex> lg(mout);
    std::cout<<msg<<std::endl;
}

/*************************CLIENT********************************************/
Client::Client(std::string address, int port): address(address), port(port){
    cont_error = 0;
    cont_nothing = 0;
    before_first_synch = true;
    while(true){
        try{
            prec_status = FileStatus::nothing;
            struct sockaddr_in sockaddrIn;
            sockaddrIn.sin_port = ntohs(port);
            sockaddrIn.sin_family = AF_INET;
            if(::inet_pton(AF_INET, address.c_str(), &sockaddrIn.sin_addr) <=0)
                throw std::runtime_error("error inet_ptn");
            sock.connect(&sockaddrIn, sizeof(sockaddrIn));
            break;
        }catch(std::runtime_error& e){
            std::cout << e.what() << std::endl;
            std::cout << "TRY TO REPAIR..." << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            cont_error++;
            if(cont_error == NUM_POSSIBLE_TRY_RESOLVE_ERROR){
                delete directory;
                exit(-3);
            }
        }
        catch(boost::filesystem::filesystem_error& e){
            std::cout << e.what() << std::endl;
            std::cout << "TRY TO REPAIR..." << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            cont_error++;
            if(cont_error == NUM_POSSIBLE_TRY_RESOLVE_ERROR){
                delete directory;
                exit(-3);
            }
        }
    }

}

void Client::sendMessage(Message &&m) {
    // Serialization
    std::stringstream sstream;
    try {
        Serializer oa(sstream);
        m.serialize(oa, 0);
        std::cout << "SERIALIZE: " << sstream.str() ;
    }
    catch (boost::archive::archive_exception& e) {
        throw std::runtime_error(e.what());
    }

    // Socket write
    std::string s(sstream.str());
    int length = s.length() + 1;
    sock.write(&length, sizeof(length), 0);
    sock.write(s.c_str(), length, 0);

    if(m.getType() == MessageType::text)
        std::cout << "\nMessage sent: " << m.getMessage() << std::endl;
    else
        std::cout << "\nSending file info: " << m.getFileWrapper().getPath().relative_path()<< std::endl;
}

Message Client::awaitMessage(size_t msg_size = SIZE_MESSAGE_TEXT, MessageType type) {
    // Socket read
    std::unique_ptr<char[]> buf = std::make_unique<char[]>(msg_size);
    int size;
    sock.read(&size, sizeof(size), 0);
    size = sock.read(buf.get(), size, 0);
    if (size == 0) throw std::runtime_error("Closed socket");

    // Unserialization
    Message m(type);
    try {
        std::stringstream sstream;
        sstream << buf.get();
        Deserializer ia(sstream);
        m.unserialize(ia, 0);
        std::cout <<"DESERIALIZE: " << sstream.str() << std::endl;
    }
    catch (boost::archive::archive_exception& e) {
        throw std::runtime_error(e.what());
    }

    if(m.getType() == MessageType::text)
        std::cout << "\nMessage received: " << m.getMessage() << std::endl;
    else
        std::cout << "\nMessage received" << std::endl;
    return m;
}

void Client::sendMessageWithResponse(std::string message, std::string response) {
    try{
        while(true){
            Message m = Message(message);
            sendMessage(std::move(m));
            //read responseawait
            m = awaitMessage();
            if(m.getMessage().compare(response) == 0) break;
        }
    }catch(boost::archive::archive_exception& e) {
        throw std::runtime_error(e.what());
    }
}


void Client::close(){
    sock.closeSocket();     
}

Client::~Client(){
    std::cout <<"Client diconnetted..." << std::endl;
    delete directory;
    close();
}

bool Client::doLogin(std::string user, std::string password){
    while(true){
        try{
            Message m = Message(MessageType::text);
            while(true){
                m = Message("LOGIN_" + user + "_" + password);
                sendMessage(std::move(m));
                m = awaitMessage();
                if(m.getMessage().compare("OK") == 0 || m.getMessage().compare("NOT_OK") == 0) break;
            }
            if(m.getMessage().compare("OK") == 0){
                this->user = user; //login effettuato con successo
                return true;
            }
            return false; //login non effettuato
        }
        catch(std::runtime_error& e){
            std::cout << e.what() << std::endl;
            std::cout << "TRY TO REPAIR..." << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            cont_error++;
            if(cont_error == NUM_POSSIBLE_TRY_RESOLVE_ERROR){
                delete directory;
                return false;
            }
        }
        catch(boost::filesystem::filesystem_error& e){
            std::cout << e.what() << std::endl;
            std::cout << "TRY TO REPAIR..." << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            cont_error++;
            if(cont_error == NUM_POSSIBLE_TRY_RESOLVE_ERROR){
                delete directory;
                return false;
            }
        }
    }
}

void Client::sendCreateFileAsynch(std::string path_to_watch){
    std::thread create([this, path_to_watch]()->void{
        std::lock_guard<std::mutex> lg(muSend);
        sendMessageWithResponse("CREATE", "ACK");
        inviaFile(path(path_to_watch), FileStatus::created, false); 
    });
    create.detach();
}

void Client::sendModifyFileAsynch(std::string path_to_watch){
    std::thread modify([this, path_to_watch]()->void{
        std::lock_guard<std::mutex> lg(muSend);
        sendMessageWithResponse("MODIFY", "ACK"); //invio richiesta modifica   
        inviaFile(path(path_to_watch), FileStatus::modified, false); 
    });
    modify.detach();
}

void Client::sendEraseFileAsynch(std::string path_to_watch){
    std::thread erase([this, path_to_watch]()->void{
        std::lock_guard<std::mutex> lg(muSend);
        sendMessageWithResponse("ERASE", "ACK");//invio richiesta cancellazione
        inviaFile(path(path_to_watch), FileStatus::erased, false); 
    });
    erase.detach();
}


void Client::sendWrapperAllRequest(FileWatcher& fw, bool first_syncro){//wrapper invio le richieste
    std::lock_guard<std::mutex> lg(mu2);
    //std::cout << "NOT EMPTY" << std::endl;
    sendAllRequest(fw);

    //while(!request.empty()) {cv_request.wait(lg);} //attendo che sia vuota la mappa
}

void Client::sendAllRequest(FileWatcher& fw, bool first_syncro){//scandisco la coda delle richieste e le invio al server che si occuperà di smistarle
    //std::lock_guard<std::mutex> lg(mu2);
    //std::cout << "LISTA" << std::endl;
    for(std::pair<std::string, FileStatus> t : request){
         switch(t.second){
             case FileStatus::created:
                 //std::cout << t.first << " create" << std::endl;
                 sendCreateFileAsynch(t.first);
                break;
             case FileStatus::modified:
                //std::cout << t.first << " modify" << std::endl;
                sendModifyFileAsynch(t.first);
                break;
             case FileStatus::erased:
                //std::cout << t.first << " erase" << std::endl;
                sendEraseFileAsynch(t.first);
                break;
             default:break;
         }
    }
    request.clear();//pulisco la coda delle richieste
    if(!first_syncro) fw.restart();//filewatcher può ripartire a caricare le richieste fino a che non ci sono richieste
    //cv_request.notify_all();
}

void Client::monitoraCartella(std::string folder){

    directory = new Folder(user, folder);
    //mi metto in ascolto su un thread separato e attendo una modifica
    FileWatcher fw{folder, std::chrono::milliseconds(2000)};
    fw.first_syncro();

    std::thread start([this, &folder, &fw] () {
        fw.start([this, &fw](std::string path_to_watch, FileStatus status, bool locked, bool first_syncro) -> void {
            std::lock_guard<std::mutex> lg(mu);
            try{
                switch(status) {
                    case FileStatus::created:{
                            std::cout << "Created: " << path_to_watch << '\n';
                            cont_nothing = 0;
                            if(!locked && (exists(path(path_to_watch))))
                            request.insert(std::make_pair(path_to_watch, status)); //carico la richiesta di creazione nella coda delle richieste
                        //}
                        break;
                    }
                    case FileStatus::modified:{
                        //dico al server che è stato modificato un file e lo invio al server
                        std::cout << "Modified: " << path_to_watch << '\n'; 
                        cont_nothing = 0;
                        if(!locked && (request.find(path_to_watch) == request.end()) && (exists(path(path_to_watch))) ){ 
                            request.insert(std::make_pair(path_to_watch, status)); //se il file non è stato creato prima e non c'è un lock
                        }
                        break;
                    }
                    case FileStatus::erased:{
                        //dico al server che è stato modificato un file e lo invio al server
                        std::cout << "Erased: " << path_to_watch << '\n';
                        //std::cout << first_syncro << " " << locked <<std::endl;
                        cont_nothing = 0; //non ho inviato le richieste ancora
                        request.insert(std::make_pair(path_to_watch, status));
                        break;
                    }
                    case FileStatus::nothing:{//invio le richieste accodate
                        if(!first_syncro){
                            fw.freeze();
                            std::cout << "NOTHING" << std::endl;
                            if(!request.empty()){
                                if(cont_nothing == 0){ 
                                  sendWrapperAllRequest(fw); //restart in sendAllRequest. Invio le richieste e intanto rendo il fileWatcer frizzato 
                                  cont_nothing++;
                                }
                            }else{
                                cont_nothing=0;
                                fw.restart();
                            }
                        }
                        break;
                    }
                    default: break;
                }
            }catch(std::runtime_error& e){
                std::cout << e.what() << std::endl;
                std::cout << "TRY TO REPAIR.." << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                cont_error++;
                if(cont_error == NUM_POSSIBLE_TRY_RESOLVE_ERROR){
                    delete directory;
                    exit(-2);
                }
            }
            catch(boost::filesystem::filesystem_error& e){
                std::cout << e.what() << std::endl;
                std::cout << "TRY TO REPAIR..." << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                cont_error++;
                if(cont_error == NUM_POSSIBLE_TRY_RESOLVE_ERROR){
                    delete directory;
                    exit(-2);
                }
            }
        }); 
    });

    //sincronizzazione iniziale
    while(true){ //ritento sincornizzazione iniziale in casi di errori per NUM.. volte, altrimenti confuto un errore permanente e chiudo il programma
        try{
            path dir(folder);           
            std::cout <<"MONITORING " << dir.string() << std::endl;
            Message m = Message("SYNC");
            sendMessage(std::move(m));
            int checksumServer = 0;
            int checksumClient;
            while(true){
                    try{
                        checksumClient = (int)directory->getChecksum();
                        break;
                    }
                    catch(...){std::cout<<"Tento checksum" << std::endl;continue;}
            }
            //attendo checksum
            sock.read(&checksumServer, sizeof(checksumServer), 0); //ricevo checksum da server
            std::cout << "CHECKSUM CLIENT " << checksumClient <<std::endl;
            std::cout << "CHECKSUM SERVER " << checksumServer <<std::endl;
            //sendMessage(Message("ACK"));
            if(checksumClient == checksumServer){ //OK
                Message m = Message("OK");
                sendMessage(std::move(m));
            }
            else if(checksumServer != 0 && checksumClient==0){ 
                //fw.not_first_syncro();
               
                fw.freeze();
                //fw.lock();
                sendMessageWithResponse("DOWNLOAD", "ACK");
                downloadDirectory(); //scarico contenuto del server
                //fw.first_syncro();
                //fw.unlock();
                fw.restart();
            }
            else{ //la cartella esiste e quindi la invio al server  
                //invio richiesta update directory server (fino ad ottenere response ACK)
                sendMessageWithResponse("UPDATE", "ACK");
                //invio solo i file con checksum diverso
                while(true){
                    try{
                        for(filesystem::path path: directory->getContent()){ 
                            path = directory->getPath()/path;
                            std::cout  << path << std::endl;
                            inviaFile(path, FileStatus::modified, false);
                        }
                        sendMessageWithResponse("END", "ACK");
                        break;
                    }
                    catch(...){std::cout<<"Tento client" << std::endl;continue;}
                }
            }
            //invio le eventuali modifiche che sono state effettuate durante la prima sincronizzazione in ordine di richiesta
            if(!request.empty())
                  sendWrapperAllRequest(fw, true);//non devo effettuare restart se metto true
            fw.not_first_syncro();

            break; //ho eseguito tutto correttamente
        }catch(std::runtime_error& e){
            std::cout << e.what() << std::endl;
            std::cout << "TRY TO REPAIR..." << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            cont_error++;
            if(cont_error == NUM_POSSIBLE_TRY_RESOLVE_ERROR){
                delete directory;
                exit(-1);
            }
        }
        catch(boost::filesystem::filesystem_error& e){
            std::cout << e.what() << std::endl;
            std::cout << "TRY TO REPAIR..." << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            cont_error++;
            if(cont_error == NUM_POSSIBLE_TRY_RESOLVE_ERROR){
                delete directory;
                exit(-1);
            }
        }
    };

    
    //syncro.join();
    start.join();
}


void Client::downloadDirectory(){
    directory->wipeFolder(); // Even if it shouldn't be needed, wipe it just to be sure
    std::cout << "Waiting directory from server" << std::endl;

    // Waiting for all files
    while (riceviFile()) {};
    sendMessage(Message("ACK"));
}

void Client::inviaFile(filesystem::path path_to_watch, FileStatus status, bool conThread){
    std::function<void(void)> fun = [this, path_to_watch, status] () -> void {
        //std::lock_guard<std::mutex> lg(mu);
        //path p(path_to_watch);
        std::stringstream ss;
        while(true){
            //std::cout << "READ PATH: "<< path_to_watch.c_str() << std::endl;
            if(is_directory(path_to_watch)){
                sendMessage(Message("DIRECTORY"));
                Message m = awaitMessage(); //attendo un response da server
                //invio directory da sincronizzare

                FileWrapper f = FileWrapper(directory->removeFolderPath(path_to_watch.string()), status, 0);//path, status, size file
               
                Message m2 = Message(std::move(f));
                //m2.print();
                sendMessage(std::move(m2));
                m = awaitMessage(); //attendo ACK per il fileWrapper

                std::cout << "DIRECTORY SEND " << directory->removeFolderPath(path_to_watch.string()) << std::endl;
                m = awaitMessage(); //attendo ACK per la fine della creazione della cartella

                //leggo messaggio da server
                if(m.getMessage().compare("ACK") == 0) break;

            }else if(is_regular_file(path_to_watch)){
                //invio file da sincronizzare
                filesystem::path relativeContent = path(directory->removeFolderPath(path_to_watch.string()));
                
                int size = directory->getFileSize(relativeContent);

                sendMessage(Message("FILE"));
                Message m = awaitMessage();  

                //invio fileInfo
                FileWrapper f = FileWrapper(relativeContent, status, size);
                Message m2 = Message(std::move(f));
                sendMessage(std::move(m2));
                m = awaitMessage(); //attendo ACK

                //invio SIZE_MESSAGE_TEXT byte alla volta
                filesystem::ifstream file;
                file.open(path_to_watch, std::ios::in | std::ios::binary);
                int cont_size=0,i=0,j=0;
                int cont_char = size;
                int num = 0;
                while(cont_char > 0){
                    std::unique_ptr<char[]> buf;
                    if(cont_char > SIZE_MESSAGE_TEXT){
                           buf = std::make_unique<char[]>(SIZE_MESSAGE_TEXT);
                           num = SIZE_MESSAGE_TEXT;
                    }
                    else{
                           buf = std::make_unique<char[]>(cont_char);
                           num = cont_char;
                    }
                    
                    if(!file.read(buf.get(), num)){
                        sendMessage(Message("FS_ERR"));
                        m = awaitMessage();
                        throw std::runtime_error("Impossible read file");
                        break;
                    };
                    std::string s(buf.get());
                    //std::cout << "INVIO DI " << num << "/" << s.length() << " CHAR" << std::endl;
                    
                    //std::cout << s <<  std::endl;
                    sock.write(buf.get(), num, 0); 
                    cont_char -= num;
                    //m = awaitMessage();
                }
                file.close();
                std::cout << "FILE SEND " << relativeContent << std::endl;

                //leggo ACK
                m = awaitMessage();
                if(m.getMessage().compare("ACK") == 0) break;

            }else if(!exists(path_to_watch)){ //file cancellato
                Message m = Message("FILE_DEL");
                sendMessage(std::move(m));
                filesystem::path relativeContent = path(directory->removeFolderPath(path_to_watch.string()));

                FileWrapper f = FileWrapper(relativeContent, status, 0);//path, status, size file (0 perche lo devo eliminare)
               
                Message m2 = Message(std::move(f));
               
                sendMessage(std::move(m2));

                m = awaitMessage(); //attendo ACK

                std::cout << "FILE DELETED " << relativeContent  << std::endl;

                //leggo ACK
                if(m.getMessage().compare("ACK") == 0) break;
                break;
            }else{
                Message m = Message("ERR");
                sendMessage(std::move(m));
                throw std::runtime_error("File not supported");
            }
        }
    };

    if(conThread){
        std::thread synch(fun);
        synch.detach();
    }else{
        fun();
    }
}

bool Client::riceviFile() {
    // Receive command and answer ACK
    Message m = awaitMessage();
    sendMessage(Message("ACK"));

    // In case we're done receiving a series
    if(m.getMessage().compare("END") == 0) return false;

    // Errors handling
    if(m.getMessage().compare("FS_ERR") == 0) throw std::runtime_error("receiveFile: File system error on client side");
    if(m.getMessage().compare("ERR") == 0) throw std::runtime_error("receiveFile: Generic error on client side");

    // Receive file info
    FileWrapper fileInfo = awaitMessage(SIZE_MESSAGE_TEXT, MessageType::file).getFileWrapper();
    sendMessage(Message("ACK"));

    // Check if the received message was a Directory or a File
    if(m.getMessage().compare("DIRECTORY") == 0 )
        switch (fileInfo.getStatus()) {
            case FileStatus::modified : // If modified it first deletes the folder, then re-creates it (so no break)
                directory->deleteFile(fileInfo.getPath());
            case FileStatus::created :
                directory->writeDirectory(fileInfo.getPath());
                break;
            case FileStatus::erased :
                directory->deleteFile(fileInfo.getPath());
                break;
            default:
                break;
        }
    else if(m.getMessage().compare("FILE") == 0 ) {
        switch (fileInfo.getStatus()) {
            case FileStatus::modified : // If modified it first deletes the folder, then re-creates it (so no break)
                directory->deleteFile(fileInfo.getPath());
            case FileStatus::created : {
                std::cout << "Reading blocks from socket" << std::endl;
                // Read chunks of data
                int count_char = fileInfo.getSize();
                int num = 0;
                while (count_char > 0) {
                    std::cout << "Remaining data: " << count_char << std::endl;
                    num = count_char > SIZE_MESSAGE_TEXT ? SIZE_MESSAGE_TEXT : count_char;
                    std::unique_ptr<char[]> buf = std::make_unique<char[]>(num);
                    int receivedSize = sock.read(buf.get(), num, 0);
                    directory->writeFile(fileInfo.getPath(), buf.get(), receivedSize);
                    count_char -= receivedSize;
                }
                break;
            }
            case FileStatus::erased :
                directory->deleteFile(fileInfo.getPath());
                break;
            default:
                break;
        }
    }
    else if (m.getMessage().compare("FILE_DEL") == 0) {
        if(!fileInfo.getPath().empty())
            directory->deleteFile(fileInfo.getPath());
    }

    // Send ACK to indicate operation success
    sendMessage(Message("ACK"));
    return true;
}
