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

//*****************************************************************************
//*****************************************************************************
void XBridgeSessionBtc::init()
{
    assert(!m_handlers.size());

    dht_random_bytes(m_myid, sizeof(m_myid));
    LOG() << "session <" << m_wallet.currency << "> generated id <"
          << util::base64_encode(std::string((char *)m_myid, sizeof(m_myid))).c_str()
          << ">";

    // process invalid
    m_handlers[xbcInvalid]               .bind(this, &XBridgeSessionBtc::processInvalid);

    m_handlers[xbcAnnounceAddresses]     .bind(this, &XBridgeSessionBtc::processAnnounceAddresses);

    // process transaction from client wallet
    // if (XBridgeExchange::instance().isEnabled())
    {
        m_handlers[xbcTransaction]           .bind(this, &XBridgeSessionBtc::processTransaction);
        m_handlers[xbcTransactionAccepting]   .bind(this, &XBridgeSessionBtc::processTransactionAccepting);
    }
    // else
    {
        m_handlers[xbcPendingTransaction]    .bind(this, &XBridgeSessionBtc::processPendingTransaction);
    }

    // transaction processing
    {
        m_handlers[xbcTransactionHold]       .bind(this, &XBridgeSessionBtc::processTransactionHold);
        m_handlers[xbcTransactionHoldApply]  .bind(this, &XBridgeSessionBtc::processTransactionHoldApply);

        m_handlers[xbcTransactionInit]       .bind(this, &XBridgeSessionBtc::processTransactionInit);
        m_handlers[xbcTransactionInitialized].bind(this, &XBridgeSessionBtc::processTransactionInitialized);

        m_handlers[xbcTransactionCreate]     .bind(this, &XBridgeSessionBtc::processTransactionCreate);
        m_handlers[xbcTransactionCreated]    .bind(this, &XBridgeSessionBtc::processTransactionCreated);

        m_handlers[xbcTransactionSignRefund]       .bind(this, &XBridgeSessionBtc::processTransactionSignRefund);
        m_handlers[xbcTransactionRefundSigned]     .bind(this, &XBridgeSessionBtc::processTransactionRefundSigned);

        m_handlers[xbcTransactionCommitStage1]     .bind(this, &XBridgeSessionBtc::processTransactionCommitStage1);
        m_handlers[xbcTransactionCommitedStage1]   .bind(this, &XBridgeSessionBtc::processTransactionCommitedStage1);

        m_handlers[xbcTransactionConfirm]    .bind(this, &XBridgeSessionBtc::processTransactionConfirm);

        m_handlers[xbcTransactionCancel]     .bind(this, &XBridgeSessionBtc::processTransactionCancel);
        m_handlers[xbcTransactionRollback]   .bind(this, &XBridgeSessionBtc::processTransactionRollback);
        m_handlers[xbcTransactionFinished]   .bind(this, &XBridgeSessionBtc::processTransactionFinished);
        m_handlers[xbcTransactionDropped]    .bind(this, &XBridgeSessionBtc::processTransactionDropped);

        m_handlers[xbcTransactionConfirmed]  .bind(this, &XBridgeSessionBtc::processTransactionConfirmed);

        // wallet received transaction
        m_handlers[xbcReceivedTransaction]   .bind(this, &XBridgeSessionBtc::processBitcoinTransactionHash);
    }

    m_handlers[xbcAddressBookEntry].bind(this, &XBridgeSessionBtc::processAddressBookEntry);

    // retranslate messages to xbridge network
    m_handlers[xbcXChatMessage].bind(this, &XBridgeSessionBtc::processXChatMessage);
}

//******************************************************************************
//******************************************************************************
//CScript destination(const std::vector<unsigned char> & address, const char prefix);
//CScript destination(const std::string & address);
//std::string txToStringBTC(const CBTCTransaction & tx);
//CBTCTransaction txFromStringBTC(const std::string & str);

//******************************************************************************
//******************************************************************************
bool XBridgeSessionBtc::processTransactionCreate(XBridgePacketPtr packet)
{
    assert(!"not implemented");
    return true;

//    DEBUG_TRACE_LOG(currencyToLog());

//    if (packet->size() != 124)
//    {
//        ERR() << "incorrect packet size for xbcTransactionCreate "
//              << "need 132 received " << packet->size() << " "
//              << __FUNCTION__;
//        return false;
//    }

//    std::vector<unsigned char> thisAddress(packet->data(), packet->data()+20);
//    std::vector<unsigned char> hubAddress(packet->data()+20, packet->data()+40);

//    // transaction id
//    uint256 id   (packet->data()+40);

//    // destination address
//    std::vector<unsigned char> destAddress(packet->data()+72, packet->data()+92);

//    // tax
//    std::vector<unsigned char> taxAddress(packet->data()+100, packet->data()+120);
//    const uint32_t taxPercent = *reinterpret_cast<uint32_t*>(packet->data()+120);

//    XBridgeTransactionDescrPtr xtx;
//    {
//        boost::mutex::scoped_lock l(XBridgeApp::m_txLocker);

//        if (!XBridgeApp::m_transactions.count(id))
//        {
//            // wtf? unknown transaction
//            LOG() << "unknown transaction " << util::to_str(id) << " " << __FUNCTION__;
//            return true;
//        }

//        xtx = XBridgeApp::m_transactions[id];
//    }

//    // lock time
//    xtx->lockTimeTx1 = *reinterpret_cast<boost::uint32_t *>(packet->data()+92);
//    xtx->lockTimeTx2 = *reinterpret_cast<boost::uint32_t *>(packet->data()+96);

//    std::vector<rpc::Unspent> entries;
//    if (!rpc::listUnspent(m_wallet.user, m_wallet.passwd, m_wallet.ip, m_wallet.port, entries))
//    {
//        LOG() << "rpc::listUnspent failed" << __FUNCTION__;
//        return true;
//    }

//    boost::uint64_t outAmount = m_wallet.COIN*(static_cast<double>(xtx->fromAmount)/XBridgeTransactionDescr::COIN);
//    boost::uint64_t taxToSend = m_wallet.COIN*(static_cast<double>(xtx->fromAmount)*taxPercent/100000/XBridgeTransactionDescr::COIN);

//    boost::uint64_t fee = 0;
//    boost::uint64_t inAmount  = 0;

//    std::vector<rpc::Unspent> usedInTx;
//    for (const rpc::Unspent & entry : entries)
//    {
//        usedInTx.push_back(entry);
//        inAmount += entry.amount*m_wallet.COIN;
//        fee = m_wallet.COIN * minTxFee(usedInTx.size(), taxToSend > 0 ? 3 : 2) / XBridgeTransactionDescr::COIN;

//        LOG() << "USED FOR TX <" << entry.txId << "> amount " << entry.amount << " " << entry.vout << " fee " << fee;

//        // check amount
//        if (inAmount >= outAmount+fee+taxToSend)
//        {
//            break;
//        }
//    }

//    // check amount
//    if (inAmount < outAmount+fee+taxToSend)
//    {
//        // no money, cancel transaction
//        sendCancelTransaction(id, crNoMoney);
//        return true;
//    }

//    // create tx1, locked
//    CBTCTransaction tx1;

//    // lock time
//    {
//        time_t local = time(NULL);// GetAdjustedTime();
//        tx1.nLockTime = local + xtx->lockTimeTx1;
//    }

//    // inputs
//    for (const rpc::Unspent & entry : usedInTx)
//    {
//        CTxIn in(COutPoint(uint256(entry.txId), entry.vout));
//        tx1.vin.push_back(in);
//    }

//    // outputs
//    tx1.vout.push_back(CTxOut(outAmount, destination(destAddress, m_wallet.addrPrefix[0])));
//    if (taxToSend)
//    {
//        tx1.vout.push_back(CTxOut(taxToSend, destination(taxAddress, m_wallet.addrPrefix[0])));
//    }

//    LOG() << "OUTPUTS <" << destination(destAddress, m_wallet.addrPrefix[0]).ToString()
//            << "> amount " << (outAmount) / m_wallet.COIN;

//    if (inAmount > outAmount)
//    {
//        std::string addr;
//        if (!rpc::getNewAddress(m_wallet.user, m_wallet.passwd, m_wallet.ip, m_wallet.port, addr))
//        {
//            // cancel transaction
//            sendCancelTransaction(id, crRpcError);
//            return true;
//        }

//        LOG() << "OUTPUTS <" << addr << "> amount " << (inAmount-outAmount-fee-taxToSend) / m_wallet.COIN;

//        // rest
//        CScript script = destination(addr);
//        tx1.vout.push_back(CTxOut(inAmount-outAmount-fee-taxToSend, script));
//    }

//    // serialize
//    std::string unsignedTx1 = txToStringBTC(tx1);
//    std::string signedTx1 = unsignedTx1;

//    bool complete = false;
//    if (!rpc::signRawTransaction(m_wallet.user, m_wallet.passwd, m_wallet.ip, m_wallet.port,
//                                 signedTx1, complete))
//    {
//        // do not sign, cancel
//        sendCancelTransaction(id, crNotSigned);
//        return true;
//    }

//    assert(complete && "not fully signed");

//    tx1 = txFromStringBTC(signedTx1);

//    LOG() << "payment tx " << tx1.GetHash().GetHex();
//    LOG() << signedTx1;

//    std::string json;
//    std::string jsonid;
//    if (rpc::decodeRawTransaction(m_wallet.user, m_wallet.passwd,
//                                  m_wallet.ip, m_wallet.port,
//                                  signedTx1, jsonid, json))
//    {
//        TXLOG() << "payment  " << json;
//    }
//    else
//    {
//        TXLOG() << "decoderawtransaction failed";
//        TXLOG() << signedTx1;
//    }

//    assert(!"not finished");

//    // xtx->payTxId = tx1.GetHash();
//    xtx->payTx   = signedTx1;

//    // create tx2
//    boost::uint64_t fee2 = minTxFee(1, 1);

//    // inputs
//    CTransaction tx2;
//    CTxIn in(COutPoint(tx1.GetHash(), 0));
//    tx2.vin.push_back(in);

//    // outputs
//    {
//        std::string addr;
//        if (!rpc::getNewAddress(m_wallet.user, m_wallet.passwd, m_wallet.ip, m_wallet.port, addr))
//        {
//            // cancel transaction
//            sendCancelTransaction(id, crRpcError);
//            return true;
//        }

//        LOG() << "OUTPUTS <" << addr << "> amount " << (outAmount-fee2) / m_wallet.COIN;

//        // rest
//        CScript script = destination(addr);
//        tx2.vout.push_back(CTxOut(outAmount-fee2, script));
//    }

//    // lock time for tx2
//    {
//        time_t local = time(0); // GetAdjustedTime();
//        tx2.nLockTime = local + xtx->lockTimeTx2;
//    }

//    // serialize
//    std::string unsignedTx2 = txToStringBTC(tx2);
//    LOG() << "revert tx (unsigned) " << tx2.GetHash().GetHex();
//    LOG() << unsignedTx2;

//    if (rpc::decodeRawTransaction(m_wallet.user, m_wallet.passwd,
//                                  m_wallet.ip, m_wallet.port,
//                                  unsignedTx2, jsonid, json))
//    {
//        TXLOG() << "rollback " << json;
//    }
//    else
//    {
//        TXLOG() << "decoderawtransaction failed";
//        TXLOG() << unsignedTx2;
//    }

//    // store
//    xtx->refTx = unsignedTx2;

//    xtx->state = XBridgeTransactionDescr::trCreated;
//    uiConnector.NotifyXBridgeTransactionStateChanged(id, xtx->state);

//    // send reply
//    XBridgePacketPtr reply(new XBridgePacket(xbcTransactionCreated));
//    reply->append(hubAddress);
//    reply->append(thisAddress);
//    reply->append(id.begin(), 32);
//    reply->append(unsignedTx1);
//    reply->append(unsignedTx2);

//    sendPacket(hubAddress, reply);
//    return true;
}

//******************************************************************************
//******************************************************************************
bool XBridgeSessionBtc::processTransactionSignRefund(XBridgePacketPtr packet)
{
    assert(!"not implemented");
    return true;

//    DEBUG_TRACE_LOG(currencyToLog());

//    if (packet->size() < 72)
//    {
//        ERR() << "incorrect packet size for xbcTransactionSign "
//              << "need more than 72 received " << packet->size() << " "
//              << __FUNCTION__;
//        return false;
//    }

//    std::vector<unsigned char> thisAddress(packet->data(), packet->data()+20);

//    size_t offset = 20;
//    std::vector<unsigned char> hubAddress(packet->data()+offset, packet->data()+offset+20);
//    offset += 20;

//    uint256 txid(packet->data()+offset);
//    offset += 32;

//    std::string rawtxpay(reinterpret_cast<const char *>(packet->data()+offset));
//    offset += rawtxpay.size()+1;

//    std::string rawtxrev(reinterpret_cast<const char *>(packet->data()+offset));

//    // check txid
//    XBridgeTransactionDescrPtr xtx;
//    {
//        boost::mutex::scoped_lock l(XBridgeApp::m_txLocker);

//        if (!XBridgeApp::m_transactions.count(txid))
//        {
//            // wtf? unknown transaction
//            LOG() << "unknown transaction " << util::to_str(txid) << " " << __FUNCTION__;
//            return true;
//        }

//        xtx = XBridgeApp::m_transactions[txid];
//    }

//    // unserialize
//    {
//        CBTCTransaction txpay = txFromStringBTC(rawtxpay);
//        CBTCTransaction txrev = txFromStringBTC(rawtxrev);

//        if (txpay.nLockTime < LOCKTIME_THRESHOLD || txrev.nLockTime < LOCKTIME_THRESHOLD)
//        {
//            // not signed, cancel tx
//            sendCancelTransaction(txid, crNotSigned);
//            return true;
//        }
//    }

//    // TODO check txpay, inputs-outputs

//    // sign txrevert
//    bool complete = false;
//    if (!rpc::signRawTransaction(m_wallet.user, m_wallet.passwd, m_wallet.ip, m_wallet.port,
//                                 rawtxrev, complete))
//    {
//        // do not sign, cancel
//        sendCancelTransaction(txid, crNotSigned);
//        return true;
//    }

//    assert(complete && "not fully signed");

//    xtx->state = XBridgeTransactionDescr::trSigned;
//    uiConnector.NotifyXBridgeTransactionStateChanged(txid, xtx->state);

//    // send reply
//    XBridgePacketPtr reply(new XBridgePacket(xbcTransactionRefundSigned));
//    reply->append(hubAddress);
//    reply->append(thisAddress);
//    reply->append(txid.begin(), 32);
//    reply->append(rawtxrev);

//    sendPacket(hubAddress, reply);
//    return true;
}
