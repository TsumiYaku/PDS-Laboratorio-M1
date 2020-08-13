#include "AuthManager.h"
#include <sqlite3.h>
#include <SimpleCrypt.h>
#include <stdexcept>
#include <functional>

#define DB_PATH "users.db"

using namespace SimpleCrypt;

bool success = false;

int callback(void *password, int argc, char **argv, char **azColName) {
    auto *pass = static_cast<std::string*>(password);
    std::string hash = encrypt(*pass);

    std::string queryHash(argv[0]);
    success = hash == queryHash;
    return 0;
}

bool AuthManager::tryLogin(const std::string& user, std::string pass) {
    sqlite3 *db;
    success = false;

    // DB Connection
    int rc = sqlite3_open(DB_PATH, &db);
    if(rc) { // Handling connection error
        sqlite3_close(db);
        throw std::runtime_error(std::string("Can't connect to db, error: " ).append(sqlite3_errmsg(db)));
    }

    // Query execution
    std::string query = std::string("SELECT Hash FROM 'Users' WHERE User='").append(user).append("';");
    char *errMsg = 0;
    rc = sqlite3_exec(db, query.c_str(), &callback, static_cast<void *>(&pass), &errMsg);
    if(rc != SQLITE_OK) { // Handling query error
        std::string s = std::string("SQL Query error: ").append(errMsg);
        sqlite3_free(errMsg);
        sqlite3_close(db);
        throw std::runtime_error(s);
    }

    sqlite3_close(db);
    return success;
}
