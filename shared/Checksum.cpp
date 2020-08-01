#include "Checksum.h"

/* Number of bytes that compose the checksum (up to 32 bits) */
#define NBYTES 4

Checksum::Checksum() {
    bitmask = 0;
    for(int i=0; i<NBYTES; i++) {
        bitmask += 0xff << (i*8);
    }
}

void Checksum::add(const char *str, int len) {
    for (int i=0; i<len; i+=NBYTES) {
        checksum = (checksum >> 1) + ((checksum & 1) << (NBYTES*8-1));
        int remaining = (len-i)%4;
        if (remaining == 0) remaining = 4;
        for(int j=0; j<remaining; j++)
            checksum += str[j] << j*8;
        checksum &= bitmask;
    }
}


void Checksum::add(const std::string& str) {
    add(str.c_str(), str.length());
}

void Checksum::reset() {
    checksum = 0;
}

uint32_t Checksum::getChecksum() {
    return checksum;
}

