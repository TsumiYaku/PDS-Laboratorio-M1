#include "Checksum.h"

void Checksum::add(const char *str, int len) {
    for (int i=0; i<len; i++) {
        checksum = (checksum >> 1) + ((checksum & 1) << 7);
        checksum += str[i];
        checksum &= 0xff;
    }
}


void Checksum::add(const std::string& str) {
    add(str.c_str(), str.length());
}

void Checksum::reset() {
    checksum = 0;
}

uint8_t Checksum::getChecksum() {
    return checksum;
}

