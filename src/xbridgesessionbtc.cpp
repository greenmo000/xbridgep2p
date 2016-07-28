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
    init();
}

//*****************************************************************************
//*****************************************************************************
XBridgeSessionBtc::XBridgeSessionBtc(const WalletParam & wallet)
    : XBridgeSession(wallet)
{
    init();
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
    assert(!m_processors.size());

    dht_random_bytes(m_myid, sizeof(m_myid));
    LOG() << "session <" << m_wallet.currency << "> generated id <"
             << util::base64_encode(std::string((char *)m_myid, sizeof(m_myid))).c_str()
             << ">";

    // process invalid
    m_processors[xbcInvalid]               .bind(this, &XBridgeSessionBtc::processInvalid);

    m_processors[xbcAnnounceAddresses]     .bind(this, &XBridgeSessionBtc::processAnnounceAddresses);

    // process transaction from client wallet
    // if (XBridgeExchange::instance().isEnabled())
    {
        m_processors[xbcTransaction]           .bind(this, &XBridgeSessionBtc::processTransaction);
        m_processors[xbcTransactionAccepting]   .bind(this, &XBridgeSessionBtc::processTransactionAccepting);
    }
    // else
    {
        m_processors[xbcPendingTransaction]    .bind(this, &XBridgeSessionBtc::processPendingTransaction);
    }

    // transaction processing
    {
        m_processors[xbcTransactionHold]       .bind(this, &XBridgeSessionBtc::processTransactionHold);
        m_processors[xbcTransactionHoldApply]  .bind(this, &XBridgeSessionBtc::processTransactionHoldApply);

        m_processors[xbcTransactionInit]       .bind(this, &XBridgeSessionBtc::processTransactionInit);
        m_processors[xbcTransactionInitialized].bind(this, &XBridgeSessionBtc::processTransactionInitialized);

        m_processors[xbcTransactionCreate]     .bind(this, &XBridgeSessionBtc::processTransactionCreate);
        m_processors[xbcTransactionCreated]    .bind(this, &XBridgeSessionBtc::processTransactionCreated);

        m_processors[xbcTransactionSign]       .bind(this, &XBridgeSessionBtc::processTransactionSign);
        m_processors[xbcTransactionSigned]     .bind(this, &XBridgeSessionBtc::processTransactionSigned);

        m_processors[xbcTransactionCommit]     .bind(this, &XBridgeSessionBtc::processTransactionCommit);
        m_processors[xbcTransactionCommited]   .bind(this, &XBridgeSessionBtc::processTransactionCommited);

        m_processors[xbcTransactionConfirm]    .bind(this, &XBridgeSessionBtc::processTransactionConfirm);

        m_processors[xbcTransactionCancel]     .bind(this, &XBridgeSessionBtc::processTransactionCancel);
        m_processors[xbcTransactionRollback]   .bind(this, &XBridgeSessionBtc::processTransactionRollback);
        m_processors[xbcTransactionFinished]   .bind(this, &XBridgeSessionBtc::processTransactionFinished);
        m_processors[xbcTransactionDropped]    .bind(this, &XBridgeSessionBtc::processTransactionDropped);

        m_processors[xbcTransactionConfirmed]  .bind(this, &XBridgeSessionBtc::processTransactionConfirmed);

        // wallet received transaction
        m_processors[xbcReceivedTransaction]   .bind(this, &XBridgeSessionBtc::processBitcoinTransactionHash);
    }

    m_processors[xbcAddressBookEntry].bind(this, &XBridgeSessionBtc::processAddressBookEntry);

    // retranslate messages to xbridge network
    m_processors[xbcXChatMessage].bind(this, &XBridgeSessionBtc::processXChatMessage);
}

//******************************************************************************
//******************************************************************************
CScript destination(const std::vector<unsigned char> & address, const char prefix);
CScript destination(const std::string & address);
std::string txToStringBTC(const CBTCTransaction & tx);
CBTCTransaction txFromStringBTC(const std::string & str);
boost::uint64_t minTxFee(const uint32_t inputCount, const uint32_t outputCount);

//******************************************************************************
//******************************************************************************
bool XBridgeSessionBtc::processTransactionCreate(XBridgePacketPtr packet)
{
    DEBUG_TRACE_LOG(currencyToLog());

    if (packet->size() != 124)
    {
        ERR() << "incorrect packet size for xbcTransactionCreate "
              << "need 132 received " << packet->size() << " "
              << __FUNCTION__;
        return false;
    }

    std::vector<unsigned char> thisAddress(packet->data(), packet->data()+20);
    std::vector<unsigned char> hubAddress(packet->data()+20, packet->data()+40);

    // transaction id
    uint256 id   (packet->data()+40);

    // destination address
    std::vector<unsigned char> destAddress(packet->data()+72, packet->data()+92);

    // tax
    std::vector<unsigned char> taxAddress(packet->data()+100, packet->data()+120);
    const uint32_t taxPercent = *reinterpret_cast<uint32_t*>(packet->data()+120);

    XBridgeTransactionDescrPtr xtx;
    {
        boost::mutex::scoped_lock l(XBridgeApp::m_txLocker);

        if (!XBridgeApp::m_transactions.count(id))
        {
            // wtf? unknown transaction
            LOG() << "unknown transaction " << util::to_str(id) << " " << __FUNCTION__;
            return true;
        }

        xtx = XBridgeApp::m_transactions[id];
    }

    // lock time
    xtx->lockTimeTx1 = *reinterpret_cast<boost::uint32_t *>(packet->data()+92);
    xtx->lockTimeTx2 = *reinterpret_cast<boost::uint32_t *>(packet->data()+96);

    std::vector<rpc::Unspent> entries;
    if (!rpc::listUnspent(m_wallet.user, m_wallet.passwd, m_wallet.ip, m_wallet.port, entries))
    {
        LOG() << "rpc::listUnspent failed" << __FUNCTION__;
        return true;
    }

    boost::uint64_t outAmount = m_wallet.COIN*(static_cast<double>(xtx->fromAmount)/XBridgeTransactionDescr::COIN);
    boost::uint64_t taxToSend = m_wallet.COIN*(static_cast<double>(xtx->fromAmount)*taxPercent/100000/XBridgeTransactionDescr::COIN);

    boost::uint64_t fee = 0;
    boost::uint64_t inAmount  = 0;

    std::vector<rpc::Unspent> usedInTx;
    for (const rpc::Unspent & entry : entries)
    {
        usedInTx.push_back(entry);
        inAmount += entry.amount*m_wallet.COIN;
        fee = m_wallet.COIN * minTxFee(usedInTx.size(), taxToSend > 0 ? 3 : 2) / XBridgeTransactionDescr::COIN;

        LOG() << "USED FOR TX <" << entry.txId << "> amount " << entry.amount << " " << entry.vout << " fee " << fee;

        // check amount
        if (inAmount >= outAmount+fee+taxToSend)
        {
            break;
        }
    }

    // check amount
    if (inAmount < outAmount+fee+taxToSend)
    {
        // no money, cancel transaction
        sendCancelTransaction(id, crNoMoney);
        return true;
    }

    // create tx1, locked
    CBTCTransaction tx1;

    // lock time
    {
        time_t local = time(NULL);// GetAdjustedTime();
        tx1.nLockTime = local + xtx->lockTimeTx1;
    }

    // inputs
    for (const rpc::Unspent & entry : usedInTx)
    {
        CTxIn in(COutPoint(uint256(entry.txId), entry.vout));
        tx1.vin.push_back(in);
    }

    // outputs
    tx1.vout.push_back(CTxOut(outAmount, destination(destAddress, m_wallet.prefix[0])));
    if (taxToSend)
    {
        tx1.vout.push_back(CTxOut(taxToSend, destination(taxAddress, m_wallet.prefix[0])));
    }

    LOG() << "OUTPUTS <" << destination(destAddress, m_wallet.prefix[0]).ToString()
            << "> amount " << (outAmount) / m_wallet.COIN;

    if (inAmount > outAmount)
    {
        std::string addr;
        if (!rpc::getNewAddress(m_wallet.user, m_wallet.passwd, m_wallet.ip, m_wallet.port, addr))
        {
            // cancel transaction
            sendCancelTransaction(id, crRpcError);
            return true;
        }

        LOG() << "OUTPUTS <" << addr << "> amount " << (inAmount-outAmount-fee-taxToSend) / m_wallet.COIN;

        // rest
        CScript script = destination(addr);
        tx1.vout.push_back(CTxOut(inAmount-outAmount-fee-taxToSend, script));
    }

    // serialize
    std::string unsignedTx1 = txToStringBTC(tx1);
    std::string signedTx1 = unsignedTx1;

    if (!rpc::signRawTransaction(m_wallet.user, m_wallet.passwd, m_wallet.ip, m_wallet.port, signedTx1))
    {
        // do not sign, cancel
        sendCancelTransaction(id, crNotSigned);
        return true;
    }

    tx1 = txFromStringBTC(signedTx1);

    LOG() << "payment tx " << tx1.GetHash().GetHex();
    LOG() << signedTx1;

    std::string json;
    if (rpc::decodeRawTransaction(m_wallet.user, m_wallet.passwd, m_wallet.ip, m_wallet.port, signedTx1, json))
    {
        TXLOG() << "payment  " << json;
    }
    else
    {
        TXLOG() << "decoderawtransaction failed";
        TXLOG() << signedTx1;
    }

    xtx->payTxId = tx1.GetHash();
    xtx->payTx   = signedTx1;

    // create tx2
    boost::uint64_t fee2 = minTxFee(1, 1);

    // inputs
    CTransaction tx2;
    CTxIn in(COutPoint(tx1.GetHash(), 0));
    tx2.vin.push_back(in);

    // outputs
    {
        std::string addr;
        if (!rpc::getNewAddress(m_wallet.user, m_wallet.passwd, m_wallet.ip, m_wallet.port, addr))
        {
            // cancel transaction
            sendCancelTransaction(id, crRpcError);
            return true;
        }

        LOG() << "OUTPUTS <" << addr << "> amount " << (outAmount-fee2) / m_wallet.COIN;

        // rest
        CScript script = destination(addr);
        tx2.vout.push_back(CTxOut(outAmount-fee2, script));
    }

    // lock time for tx2
    {
        time_t local = time(0); // GetAdjustedTime();
        tx2.nLockTime = local + xtx->lockTimeTx2;
    }

    // serialize
    std::string unsignedTx2 = txToStringBTC(tx2);
    LOG() << "revert tx (unsigned) " << tx2.GetHash().GetHex();
    LOG() << unsignedTx2;

    if (rpc::decodeRawTransaction(m_wallet.user, m_wallet.passwd, m_wallet.ip, m_wallet.port, unsignedTx2, json))
    {
        TXLOG() << "rollback " << json;
    }
    else
    {
        TXLOG() << "decoderawtransaction failed";
        TXLOG() << unsignedTx2;
    }

    // store
    xtx->revTx = unsignedTx2;

    xtx->state = XBridgeTransactionDescr::trCreated;
    uiConnector.NotifyXBridgeTransactionStateChanged(id, xtx->state);

    // send reply
    XBridgePacketPtr reply(new XBridgePacket(xbcTransactionCreated));
    reply->append(hubAddress);
    reply->append(thisAddress);
    reply->append(id.begin(), 32);
    reply->append(unsignedTx1);
    reply->append(unsignedTx2);

    sendPacket(hubAddress, reply);
    return true;
}

//******************************************************************************
//******************************************************************************
bool XBridgeSessionBtc::processTransactionSign(XBridgePacketPtr packet)
{
    DEBUG_TRACE_LOG(currencyToLog());

    if (packet->size() < 72)
    {
        ERR() << "incorrect packet size for xbcTransactionSign "
              << "need more than 72 received " << packet->size() << " "
              << __FUNCTION__;
        return false;
    }

    std::vector<unsigned char> thisAddress(packet->data(), packet->data()+20);

    size_t offset = 20;
    std::vector<unsigned char> hubAddress(packet->data()+offset, packet->data()+offset+20);
    offset += 20;

    uint256 txid(packet->data()+offset);
    offset += 32;

    std::string rawtxpay(reinterpret_cast<const char *>(packet->data()+offset));
    offset += rawtxpay.size()+1;

    std::string rawtxrev(reinterpret_cast<const char *>(packet->data()+offset));

    // check txid
    XBridgeTransactionDescrPtr xtx;
    {
        boost::mutex::scoped_lock l(XBridgeApp::m_txLocker);

        if (!XBridgeApp::m_transactions.count(txid))
        {
            // wtf? unknown transaction
            LOG() << "unknown transaction " << util::to_str(txid) << " " << __FUNCTION__;
            return true;
        }

        xtx = XBridgeApp::m_transactions[txid];
    }

    // unserialize
    {
        CBTCTransaction txpay = txFromStringBTC(rawtxpay);
        CBTCTransaction txrev = txFromStringBTC(rawtxrev);

        if (txpay.nLockTime < LOCKTIME_THRESHOLD || txrev.nLockTime < LOCKTIME_THRESHOLD)
        {
            // not signed, cancel tx
            sendCancelTransaction(txid, crNotSigned);
            return true;
        }
    }

    // TODO check txpay, inputs-outputs

    // sign txrevert
    if (!rpc::signRawTransaction(m_wallet.user, m_wallet.passwd, m_wallet.ip, m_wallet.port, rawtxrev))
    {
        // do not sign, cancel
        sendCancelTransaction(txid, crNotSigned);
        return true;
    }

    xtx->state = XBridgeTransactionDescr::trSigned;
    uiConnector.NotifyXBridgeTransactionStateChanged(txid, xtx->state);

    // send reply
    XBridgePacketPtr reply(new XBridgePacket(xbcTransactionSigned));
    reply->append(hubAddress);
    reply->append(thisAddress);
    reply->append(txid.begin(), 32);
    reply->append(rawtxrev);

    sendPacket(hubAddress, reply);
    return true;
}
