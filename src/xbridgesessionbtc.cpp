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
std::pair<uint32_t, uint32_t> XBridgeSessionBtc::lockTime(const char role) const
{
//    rpc::Info info;
//    if (!rpc::getInfo(m_wallet.user, m_wallet.passwd,
//                      m_wallet.ip, m_wallet.port, info))
//    {
//        LOG() << "blockchain info not received " << __FUNCTION__;
//        return std::make_pair(0, 0);
//    }

//    if (info.blocks == 0)
//    {
//        LOG() << "block count not defined in blockchain info " << __FUNCTION__;
//        return std::make_pair(0, 0);
//    }

    // lock time
    uint32_t lt = 0;
    if (role == 'A')
    {
        // 72h in seconds
        // lt = info.blocks + 1;// 259200 / m_wallet.blockTime;
        lt = 259200 / m_wallet.blockTime;
    }
    else if (role == 'B')
    {
        // 36h in seconds
        // lt = info.blocks + 1;// 259200 / 2 / m_wallet.blockTime;
        lt = 259200 / 2 / m_wallet.blockTime;
    }

    uint32_t seq = lt;
    lt = seq & 0x0000ffff;
    return std::make_pair(seq, lt);

}

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
    tx->nVersion  = 2;
    tx->nLockTime = lockTime;

//    uint32_t sequence = lockTime ? std::numeric_limits<uint32_t>::max() - 1 : std::numeric_limits<uint32_t>::max();

//    for (const std::pair<std::string, int> & in : inputs)
//    {
//        tx->vin.push_back(CTxIn(COutPoint(uint256(in.first), in.second),
//                                CScript(), sequence));
//    }
    for (const std::pair<std::string, int> & in : inputs)
    {
        tx->vin.push_back(CTxIn(COutPoint(uint256(in.first), in.second)));
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
