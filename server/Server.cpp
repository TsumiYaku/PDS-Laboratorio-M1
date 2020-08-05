#include "Server.h"
#include "Folder.h"
#include <iostream>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <libnet.h>

using Serializer = boost::archive::text_oarchive;
using Deserializer = boost::archive::text_iarchive;

Server::Server(int port): ss(port) {
    for(int i=0; i<POOL_SIZE; i++) {
        std::thread t(handleConnection, this);
        t.detach();
        pool.push_back(std::move(t));
    }
}

void Server::run() {
    std::cout << "Server ready, waiting for connections..." << std::endl;
    while(true) {
        // Read incoming connections
        struct sockaddr_in addr;
        unsigned int len = sizeof(addr);
        Socket s = ss.accept(&addr, &len);
        char name[16];
        if(inet_ntop(AF_INET, &addr.sin_addr, name, sizeof(name)) == nullptr) throw std::runtime_error("Error during address name conversion");
        std::cout << "Client " << name << ":" << ntohs(addr.sin_port) << "connected" << std::endl;

        // Adds the connection to the thread pool's queue
        Connection conn(std::move(s));
        enqueueConnection(std::move(conn));
    }
}

void Server::enqueueConnection(Connection&& conn) {
    std::lock_guard<std::mutex> lg(pool_m);
    connections.push(std::move(conn));
    pool_cv.notify_all(); // notify threads that a new connection is ready to be handled
}

Connection Server::dequeueConnection() {
    std::unique_lock<std::mutex> lg(pool_m);
    while(connections.empty()) // wait for a connection to handle
        pool_cv.wait(lg);

    Connection conn = std::move(connections.front());
    connections.pop();
    return conn;
}

void Server::handleConnection(Server* server) {
    while(true) {
        Connection conn = server->dequeueConnection(); // blocking call with cv
        conn.run();
    }
}
