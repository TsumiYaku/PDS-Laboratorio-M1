#include "SimpleCrypt.h"

char key[7] = {'A', 'W', 'C', 'K', 'L', 'Q', 'M'}; //Any chars will work

std::string SimpleCrypt::encrypt(std::string toEncrypt) {
    std::string output = toEncrypt;
    for (int i = 0; i < toEncrypt.size(); i++)
        output[i] = toEncrypt[i] ^ key[i % (sizeof(key) / sizeof(char))];
    return output;
}

std::string SimpleCrypt::decrypt(std::string toDecrypt) {
    std::string output = toDecrypt;
    for (int i = 0; i < toDecrypt.size(); i++)
        output[i] = toDecrypt[i] ^ key[i % (sizeof(key) / sizeof(char))];
    return output;
}


