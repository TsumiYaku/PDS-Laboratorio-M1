#pragma once

#include <cstdint>
#include <string>

/* Implementation of a n-bits (8/16/24/32 bits) checksum using BSD checksum algorithm:
 * en.wikipedia.org/wiki/BSD_checksum */


class Checksum {
private:
    uint32_t checksum = 0;
    uint32_t bitmask;

public:
    Checksum();
    void add(const char* str, int len);
    void add(const std::string& str);
    void reset();
    uint32_t getChecksum();
};


