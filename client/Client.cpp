#include "Client.h"
#include <communication/Exchanger.h>

std::mutex mout;
void log(std::string msg){
    std::lock_guard<std::mutex> lg(mout);
    std::cout<<msg<<std::endl;
}

/*************************CLIENT********************************************/
Client::Client(std::string address, int port): address(address), port(port){
    cont_error = 0;
    cont_nothing = 0;

    while(true){
        try{
            struct sockaddr_in sockaddrIn;
            sockaddrIn.sin_port = ntohs(port);
            sockaddrIn.sin_family = AF_INET;
            if(::inet_pton(AF_INET, address.c_str(), &sockaddrIn.sin_addr) <=0)
                throw std::runtime_error("error inet_ptn");
            sock.connect(&sockaddrIn, sizeof(sockaddrIn));
            break;
        }catch(std::runtime_error& e){
            log(e.what());
            log("TRY TO REPAIR...");
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            cont_error++;
            if(cont_error == NUM_POSSIBLE_TRY_RESOLVE_ERROR){
                delete directory;
                close();
                exit(-3);
            }
        }
        catch(boost::filesystem::filesystem_error& e){
            log(e.what());
            log("TRY TO REPAIR...");
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            cont_error++;
            if(cont_error == NUM_POSSIBLE_TRY_RESOLVE_ERROR){
                delete directory;
                close();
                exit(-3);
            }
        }
    }
}

void Client::sendMessage(Message &&m) {
    MessageExchanger::sendMessage(&sock, std::move(m));
}

Message Client::awaitMessage(size_t msg_size = SIZE_MESSAGE_TEXT, MessageType type) {
    return MessageExchanger::awaitMessage(&sock, msg_size, type);
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
    log("Client disconnession...");
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
            log(e.what());
            log("TRY TO REPAIR...");
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            cont_error++;
            if(cont_error == NUM_POSSIBLE_TRY_RESOLVE_ERROR){
                delete directory;
                close();
                return false;
            }
        }
        catch(boost::filesystem::filesystem_error& e){
            log("TRY TO REPAIR...");
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            cont_error++;
            if(cont_error == NUM_POSSIBLE_TRY_RESOLVE_ERROR){
                delete directory;
                close();
                return false;
            }
        }
    }
}

void Client::sendCreateFileAsynch(std::string path_to_watch){
    std::thread create([this, path_to_watch]()->void{
        std::lock_guard<std::mutex> lg(muSend);
        sendMessageWithResponse("CREATE", "ACK"); //invio richiesta creazione
        FileExchanger::sendFile(&sock, directory, path(directory->removeFolderPath(path_to_watch)), FileStatus::created);
    });
    create.detach();
}

void Client::sendModifyFileAsynch(std::string path_to_watch){
    std::thread modify([this, path_to_watch]()->void{
        std::lock_guard<std::mutex> lg(muSend);
        sendMessageWithResponse("MODIFY", "ACK"); //invio richiesta modifica   
        FileExchanger::sendFile(&sock, directory, path(directory->removeFolderPath(path_to_watch)), FileStatus::modified);
    });
    modify.detach();
}

void Client::sendEraseFileAsynch(std::string path_to_watch){
    std::thread erase([this, path_to_watch]()->void{
        std::lock_guard<std::mutex> lg(muSend);
        sendMessageWithResponse("ERASE", "ACK");//invio richiesta cancellazione
        FileExchanger::sendFile(&sock, directory, path(directory->removeFolderPath(path_to_watch)), FileStatus::erased);
    });
    erase.detach();
}

//wrapper invio richieste client.
void Client::sendWrapperAllRequest(FileWatcher& fw, bool first_syncro){
    std::lock_guard<std::mutex> lg(mu2);
    sendAllRequest(fw);
}

//scandisco la coda delle richieste e le invio al server che si occuperà di smistarle
//sblocca il filewatcer se non si è in fase di prima sincronizzazione
void Client::sendAllRequest(FileWatcher& fw, bool first_syncro){
    for(std::pair<std::string, FileStatus> t : request){
         switch(t.second){
             case FileStatus::created:
                 sendCreateFileAsynch(t.first);
                break;
             case FileStatus::modified:
                sendModifyFileAsynch(t.first);
                break;
             case FileStatus::erased:
                sendEraseFileAsynch(t.first);
                break;
             default:break;
         }
    }
    request.clear();//pulisco la coda delle richieste
    if(!first_syncro) 
         fw.restart();//filewatcher può ripartire a caricare le richieste fino a che non si è in stato di nothing(nessuna modifica della cartella da monitorare)
   
}

void Client::monitoraCartella(std::string folder){

    directory = new Folder(user, folder);//directory da monitorare
    
    FileWatcher fw{folder, std::chrono::milliseconds(TIME_MONITORING)};

    fw.first_syncro();//se si è in fase di syncro aggiungo richieste alla coda delle richieste che verranno smistate al termine della prima sincronizzazione

    std::thread start([this, &folder, &fw] () {
        fw.start([this, &fw](std::string path_to_watch, FileStatus status, bool first_syncro) -> void {
            std::lock_guard<std::mutex> lg(mu);
            try{
                switch(status) {
                    case FileStatus::created:{
                            log("Created: " + path_to_watch);
                            cont_nothing = 0;
                            if(!fw.getLocked() && (exists(path(path_to_watch))))
                               request.insert(std::make_pair(path_to_watch, status)); //carico la richiesta di creazione nella coda delle richieste
                        //}
                        break;
                    }
                    case FileStatus::modified:{
                        //dico al server che è stato modificato un file e lo invio al server
                        log("Modified: " + path_to_watch); 
                        cont_nothing = 0;
                        if(!fw.getLocked() && (request.find(path_to_watch) == request.end()) && (exists(path(path_to_watch))) ){ 
                            request.insert(std::make_pair(path_to_watch, status)); //se il file non è stato creato prima e non c'è un lock
                        }
                        break;
                    }
                    case FileStatus::erased:{
                        //dico al server che è stato modificato un file e lo invio al server
                        log("Erased: " + path_to_watch);

                        cont_nothing = 0; //non ho inviato le richieste ancora
                        if(!fw.getLocked())
                           request.insert(std::make_pair(path_to_watch, status));
                        break;
                    }
                    case FileStatus::nothing:{//invio le richieste accodate
                        if(!first_syncro){
                            fw.freeze(); //"blocco" il filewatcher
                            if(!request.empty()){
                                if(cont_nothing == 0){ 
                                  sendWrapperAllRequest(fw); //smisto le richieste al server 
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
                log(e.what());
                log("TRY TO REPAIR..");
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                cont_error++;
                if(cont_error == NUM_POSSIBLE_TRY_RESOLVE_ERROR){
                    delete directory;
                    close();
                    exit(-2);
                }
            }
            catch(boost::filesystem::filesystem_error& e){
                log(e.what());
                log("TRY TO REPAIR..");
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                cont_error++;
                if(cont_error == NUM_POSSIBLE_TRY_RESOLVE_ERROR){
                    delete directory;
                    close();
                    exit(-2);
                }
            }
        }); 
    });

    //sincronizzazione iniziale
    while(true){ //ritento sincornizzazione iniziale in casi di errori per NUM.. volte, altrimenti confuto un errore permanente e chiudo il processo
        try{
            path dir(folder);           
            log("MONITORING " + dir.string());
            Message m = Message("SYNC");
            sendMessage(std::move(m));
            int checksumServer = 0;
            int checksumClient;
            while(true){//in caso di errore
                    try{
                        checksumClient = (int)directory->getChecksum();
                        break;
                    }
                    catch(...){continue;}
            }
            //attendo checksum
            sock.read(&checksumServer, sizeof(checksumServer), 0); //ricevo checksum da server

            if(checksumClient == checksumServer){ //cartelle sincronizzate
                Message m = Message("OK");
                sendMessage(std::move(m));
            }
            else if(checksumServer != 0 && checksumClient==0){ //client non ha cartella e la scarica dal server
                //download_file_first_Syncro = true;
                fw.lock();
                sendMessageWithResponse("DOWNLOAD", "ACK");
                downloadDirectory(); //scarico contenuto del server
                request.clear(); //per sicurezza se il lock non è andato a buon fine
                fw.unlock();
            }
            else{ //la cartella esiste e quindi la invio al server  
                //invio richiesta update directory server (fino ad ottenere response ACK)
                sendMessageWithResponse("UPDATE", "ACK");
                
                while(true){
                    try{
                        for(filesystem::path path_: directory->getContent()){ 
                            path_ = directory->getPath()/path_;
                            FileExchanger::sendFile(&sock, directory, filesystem::path(directory->removeFolderPath(path_.string())), FileStatus::modified);
                        }
                        sendMessageWithResponse("END", "ACK");
                        break;
                    }
                    catch(...){continue;}
                }
            }
            //invio le eventuali modifiche che sono state effettuate durante la prima sincronizzazione in ordine di richiesta
            if(!request.empty())
                  sendWrapperAllRequest(fw, true);//non devo effettuare restart se metto true
            fw.not_first_syncro();

            break; //ho eseguito tutto correttamente
        }catch(std::runtime_error& e){
            log(e.what());
            log("TRY TO REPAIR...");
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            cont_error++;
            if(cont_error == NUM_POSSIBLE_TRY_RESOLVE_ERROR){
                delete directory;
                close();
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
                close();
                exit(-1);
            }
        }
    };
    start.join();
}

//scarica il contenuto dal file
void Client::downloadDirectory(){
    directory->wipeFolder(); //cancello il contenuto della cartella client (solo per sicurezza)
    std::cout << "Waiting directory from server" << std::endl;

    // Waiting for all files
    while (FileExchanger::receiveFile(&sock, directory)) {};
    sendMessage(Message("ACK"));
}
