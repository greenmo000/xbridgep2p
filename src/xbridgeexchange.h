//*****************************************************************************
//*****************************************************************************

#ifndef XBRIDGEEXCHANGE_H
#define XBRIDGEEXCHANGE_H

#include "util/uint256.h"
#include "xbridgetransaction.h"

#include <string>
#include <set>
#include <map>

#include <boost/cstdint.hpp>
#include <boost/thread/mutex.hpp>

//*****************************************************************************
//*****************************************************************************
typedef std::pair<std::string, std::string> StringPair;

//*****************************************************************************
//*****************************************************************************
struct WalletParam
{
    std::string title;
    std::string address;
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
    bool init();

    bool isEnabled();
    bool haveConnectedWallet(const std::string & walletName);

    bool createTransaction(const std::vector<unsigned char> & sourceAddr,
                           const boost::uint32_t sourceAmount,
                           const std::vector<unsigned char> & destAddr,
                           const boost::uint32_t destAmount,
                           uint256 & transactionId);
    bool updateTransaction(const uint256 & hash);

    const XBridgeTransactionPtr transaction(const uint256 & hash);

    std::vector<StringPair> listOfWallets() const;

private:
    // connected wallets
    typedef std::map<std::string, WalletParam> WalletList;
    WalletList                               m_wallets;

    boost::mutex                             m_transactionsLock;
    std::map<uint256, XBridgeTransactionPtr> m_transactions;

    std::set<uint256>                        m_walletTransactions;
};

#endif // XBRIDGEEXCHANGE_H
