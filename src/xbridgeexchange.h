//*****************************************************************************
//*****************************************************************************

#ifndef XBRIDGEEXCHANGE_H
#define XBRIDGEEXCHANGE_H

#include "util/uint256.h"

#include <string>
#include <set>
#include <map>

//*****************************************************************************
//*****************************************************************************
class Transaction
{

};

//*****************************************************************************
//*****************************************************************************
class XBridgeExchange
{
public:
    static XBridgeExchange & instance();

protected:
    XBridgeExchange();
    ~XBridgeExchange();

public:
    bool isEnabled();
    bool haveConnectedWallet(const std::string & walletName);

    bool createTransaction();
    bool updateTransaction(const uint256 & hash);

private:
    std::map<uint256, Transaction> m_transactions;

    std::set<std::string> m_wallets;
};

#endif // XBRIDGEEXCHANGE_H
