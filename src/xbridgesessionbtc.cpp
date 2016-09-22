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
                                                    const CPubKey & my1,
                                                    const CPubKey & r1,
                                                    const CPubKey & x,
                                                    const std::vector<std::pair<std::string, double> > & outputs,
                                                    const uint32_t lockTime)
{
    CBTCTransaction tx;

    for (const std::pair<std::string, int> & in : inputs)
    {
        tx.vin.push_back(CTxIn(COutPoint(uint256(in.first), in.second),
                               CScript(), std::numeric_limits<uint32_t>::max() - 1));
    }

    // for (const std::pair<std::string, double> & out : outputs)
    for (uint32_t i = 0; i < outputs.size(); ++i)
    {
        const std::pair<std::string, double> & out = outputs[i];

        CScript addr;

        if (i == 0)
        {
            addr << OP_IF << lockTime << OP_CHECKLOCKTIMEVERIFY << OP_DROP
                 << OP_DUP << OP_HASH160 << my1.GetID()
                 << OP_EQUALVERIFY << OP_CHECKSIG
                 << OP_ELSE
                 << OP_DUP << OP_HASH160 << r1.GetID()
                 << OP_EQUALVERIFY << OP_CHECKSIGVERIFY
                 << OP_DUP << OP_HASH160 << x.GetID()
                 << OP_EQUAL << OP_ENDIF;
        }
        else
        {
            addr.SetDestination(CBitcoinAddress(out.first).Get());
        }

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
