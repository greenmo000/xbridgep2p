//*****************************************************************************
//*****************************************************************************

#ifndef UTIL_H
#define UTIL_H

#include "uint256.h"

#include <string>

#include <openssl/sha.h>

//*****************************************************************************
//*****************************************************************************
namespace util
{
    // TODO make std::vector<unsigned char> instead of strings
    std::string base64_encode(const std::string& s);
    std::string base64_decode(const std::string& s);

    template<typename T1> uint256 hash(const T1 pbegin, const T1 pend)
    {
        static unsigned char pblank[1];
        uint256 hash1;
        SHA256((pbegin == pend ? pblank : (unsigned char*)&pbegin[0]), (pend - pbegin) * sizeof(pbegin[0]), (unsigned char*)&hash1);
        uint256 hash2;
        SHA256((unsigned char*)&hash1, sizeof(hash1), (unsigned char*)&hash2);
        return hash2;
    }
}

#endif // UTIL_H
