#pragma once

#include <cstdint>
#include <string>

/* Implementation of a 16 bits checksum using BSD checksum algorithm:
 * en.wikipedia.org/wiki/BSD_checksum */

class Checksum {
private:
    uint8_t checksum = 0;

public:
    void add(const char* str, int len);
    void add(const std::string& str);
    void reset();
    uint8_t getChecksum();
};


