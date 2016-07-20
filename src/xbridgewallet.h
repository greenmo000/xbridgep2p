//*****************************************************************************
//*****************************************************************************

#ifndef XBRIDGEWALLET_H
#define XBRIDGEWALLET_H

#include <string>
#include <vector>
#include <stdint.h>

//*****************************************************************************
//*****************************************************************************
struct WalletParam
{
    std::string                title;
    std::string                currency;
    std::string                address;
    std::string                ip;
    std::string                port;
    std::string                user;
    std::string                passwd;
    std::string                prefix;
    std::vector<unsigned char> feeaddr;
    unsigned int               fee;
    uint64_t                   COIN;
    uint64_t                   minAmount;
    uint64_t                   dustAmount;

    WalletParam()
        : fee(300)
        , COIN(0)
        , minAmount(0)
        , dustAmount(0)
    {
    }
};


#endif // XBRIDGEWALLET_H
