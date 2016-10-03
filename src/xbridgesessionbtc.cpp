//*****************************************************************************
//*****************************************************************************

// #include <boost/asio.hpp>
// #include <boost/asio/buffer.hpp>
#include <boost/algorithm/string.hpp>

#include "xbridgesessionbtc.h"
#include "xbridgeapp.h"
#include "xbridgeexchange.h"
#include "uiconnector.h"
#include "util/util.h"
#include "util/logger.h"
#include "util/txlog.h"
#include "dht/dht.h"
#include "bitcoinrpc.h"
#include "ctransaction.h"
#include "base58.h"

//*****************************************************************************
//*****************************************************************************
XBridgeSessionBtc::XBridgeSessionBtc()
    : XBridgeSession()
{
}

//*****************************************************************************
//*****************************************************************************
XBridgeSessionBtc::XBridgeSessionBtc(const WalletParam & wallet)
    : XBridgeSession(wallet)
{
}

//*****************************************************************************
//*****************************************************************************
XBridgeSessionBtc::~XBridgeSessionBtc()
{

}

//******************************************************************************
//******************************************************************************
//uint32_t XBridgeSessionBtc::lockTime(const char role) const
//{
//    // lock time
//    uint32_t lt = 0;
//    if (role == 'A')
//    {
//        lt = 259200; // 72h in seconds
//    }
//    else if (role == 'B')
//    {
//        lt  = 259200/2; // 36h in seconds
//    }

//    lt = (1 << 22) | ((lt >> 9) + 1);
//    return lt;
//}

//******************************************************************************
//******************************************************************************
CTransactionPtr XBridgeSessionBtc::createTransaction()
{
    return CTransactionPtr(new CBTCTransaction);
}

//******************************************************************************
//******************************************************************************
CTransactionPtr XBridgeSessionBtc::createTransaction(const std::vector<std::pair<std::string, int> > & inputs,
                                                     const std::vector<std::pair<CScript, double> > &outputs,
                                                     const uint32_t lockTime)
{
    CTransactionPtr tx(new CBTCTransaction);
    tx->nLockTime = lockTime;

    uint32_t sequence = lockTime ? std::numeric_limits<uint32_t>::max() - 1 : std::numeric_limits<uint32_t>::max();

    for (const std::pair<std::string, int> & in : inputs)
    {
        tx->vin.push_back(CTxIn(COutPoint(uint256(in.first), in.second),
                                CScript(), sequence));
    }

    for (const std::pair<CScript, double> & out : outputs)
    {
        tx->vout.push_back(CTxOut(out.second*m_wallet.COIN, out.first));
    }

    return tx;
}

//*****************************************************************************
//*****************************************************************************
std::string XBridgeSessionBtc::createRawTransaction(const std::vector<std::pair<std::string, int> > & inputs,
                                                    const std::vector<std::pair<CScript, double> > &outputs,
                                                    const uint32_t lockTime)
{
    CTransactionPtr tx(createTransaction(inputs, outputs, lockTime));
    return tx->toString();
}
