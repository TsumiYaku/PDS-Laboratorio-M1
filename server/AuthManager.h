#pragma once

#include <string>

namespace AuthManager {
    bool tryLogin(const std::string& user, std::string pass);
};


