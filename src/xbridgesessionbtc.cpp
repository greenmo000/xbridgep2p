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

//*****************************************************************************
//*****************************************************************************
std::string XBridgeSessionBtc::createRawTransaction(const std::vector<std::pair<std::string, int> > & inputs,
                                                    const std::vector<std::pair<std::string, double> > & outputs,
                                                    const uint32_t lockTime)
{
//    uint32_t sequence = (1 << 22) | (lockTime >> 9);
//    uint32_t time = (sequence & 0x0000ffff) << 9;

    CBTCTransaction tx;
    tx.nVersion = 2;

    for (const std::pair<std::string, int> & in : inputs)
    {
//        tx.vin.push_back(CTxIn(COutPoint(uint256(in.first), in.second), CScript(), sequence));
        tx.vin.push_back(CTxIn(COutPoint(uint256(in.first), in.second),
                               CScript(), std::numeric_limits<uint32_t>::max() - 1));
    }

    for (const std::pair<std::string, double> & out : outputs)
    {
        CKeyID id;
        CBitcoinAddress(out.first).GetKeyID(id);
        CScript addr;
        addr << lockTime << OP_CHECKLOCKTIMEVERIFY << OP_DROP
             << OP_DUP << OP_HASH160 << id << OP_EQUALVERIFY << OP_CHECKSIG;

//        CScript addr;
//        addr.SetDestination(CBitcoinAddress(out.first).Get());

        tx.vout.push_back(CTxOut(out.second*m_wallet.COIN, addr));
    }

    if (lockTime)
    {
        tx.nLockTime = lockTime;
    }

    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << tx;
    return HexStr(ss.begin(), ss.end());
}
