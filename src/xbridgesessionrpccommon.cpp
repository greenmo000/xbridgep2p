//*****************************************************************************
//*****************************************************************************

// #include <boost/asio.hpp>
// #include <boost/asio/buffer.hpp>
#include <boost/algorithm/string.hpp>

#include "xbridgesessionrpccommon.h"
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
XBridgeSessionRpc::XBridgeSessionRpc()
    : XBridgeSession()
{
    init();
}

//*****************************************************************************
//*****************************************************************************
XBridgeSessionRpc::XBridgeSessionRpc(const WalletParam & wallet)
    : XBridgeSession(wallet)
{
    init();
}

//*****************************************************************************
//*****************************************************************************
XBridgeSessionRpc::~XBridgeSessionRpc()
{

}

//*****************************************************************************
//*****************************************************************************
void XBridgeSessionRpc::init()
{
    assert(!m_processors.size());

    dht_random_bytes(m_myid, sizeof(m_myid));
    LOG() << "session <" << m_wallet.currency << "> generated id <"
             << util::base64_encode(std::string((char *)m_myid, sizeof(m_myid))).c_str()
             << ">";

    // process invalid
    m_processors[xbcInvalid]               .bind(this, &XBridgeSessionRpc::processInvalid);

    m_processors[xbcAnnounceAddresses]     .bind(this, &XBridgeSessionRpc::processAnnounceAddresses);

    // process transaction from client wallet
    // if (XBridgeExchange::instance().isEnabled())
    {
        m_processors[xbcTransaction]           .bind(this, &XBridgeSessionRpc::processTransaction);
        m_processors[xbcTransactionAccepting]   .bind(this, &XBridgeSessionRpc::processTransactionAccepting);
    }
    // else
    {
        m_processors[xbcPendingTransaction]    .bind(this, &XBridgeSessionRpc::processPendingTransaction);
    }

    // transaction processing
    {
        m_processors[xbcTransactionHold]       .bind(this, &XBridgeSessionRpc::processTransactionHold);
        m_processors[xbcTransactionHoldApply]  .bind(this, &XBridgeSessionRpc::processTransactionHoldApply);

        m_processors[xbcTransactionInit]       .bind(this, &XBridgeSessionRpc::processTransactionInit);
        m_processors[xbcTransactionInitialized].bind(this, &XBridgeSessionRpc::processTransactionInitialized);

        m_processors[xbcTransactionCreate]     .bind(this, &XBridgeSessionRpc::processTransactionCreate);
        m_processors[xbcTransactionCreated]    .bind(this, &XBridgeSessionRpc::processTransactionCreated);

        m_processors[xbcTransactionSign]       .bind(this, &XBridgeSessionRpc::processTransactionSign);
        m_processors[xbcTransactionSigned]     .bind(this, &XBridgeSessionRpc::processTransactionSigned);

        m_processors[xbcTransactionCommit]     .bind(this, &XBridgeSessionRpc::processTransactionCommit);
        m_processors[xbcTransactionCommited]   .bind(this, &XBridgeSessionRpc::processTransactionCommited);

        m_processors[xbcTransactionConfirm]    .bind(this, &XBridgeSessionRpc::processTransactionConfirm);

        m_processors[xbcTransactionCancel]     .bind(this, &XBridgeSessionRpc::processTransactionCancel);
        m_processors[xbcTransactionRollback]   .bind(this, &XBridgeSessionRpc::processTransactionRollback);
        m_processors[xbcTransactionFinished]   .bind(this, &XBridgeSessionRpc::processTransactionFinished);
        m_processors[xbcTransactionDropped]    .bind(this, &XBridgeSessionRpc::processTransactionDropped);

        m_processors[xbcTransactionConfirmed]  .bind(this, &XBridgeSessionRpc::processTransactionConfirmed);

        // wallet received transaction
        m_processors[xbcReceivedTransaction]   .bind(this, &XBridgeSessionRpc::processBitcoinTransactionHash);
    }

    m_processors[xbcAddressBookEntry].bind(this, &XBridgeSessionRpc::processAddressBookEntry);

    // retranslate messages to xbridge network
    m_processors[xbcXChatMessage].bind(this, &XBridgeSessionRpc::processXChatMessage);
}

//******************************************************************************
//******************************************************************************
CScript destination(const std::vector<unsigned char> & address, const char prefix);
CScript destination(const std::string & address);
std::string txToString(const CTransaction & tx);
CTransaction txFromString(const std::string & str);
boost::uint64_t minTxFee(const uint32_t inputCount, const uint32_t outputCount);

//******************************************************************************
//******************************************************************************
bool XBridgeSessionRpc::processTransactionCreate(XBridgePacketPtr packet)
{
    DEBUG_TRACE_LOG(currencyToLog());

    assert(!"not implemented");
    ERR() << "not implemented ";
    return false;

    if (packet->size() != 124)
    {
        ERR() << "incorrect packet size for xbcTransactionCreate "
              << "need 124 received " << packet->size() << " "
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
    const uint32_t taxPercent = *reinterpret_cast<uint32_t *>(packet->data()+120);

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
    boost::uint64_t taxToSend = m_wallet.COIN*(static_cast<double>(xtx->fromAmount*taxPercent/100000)/XBridgeTransactionDescr::COIN);

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
    CTransaction tx1;

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
          << "> amount "
          << (outAmount) / m_wallet.COIN;

    if (inAmount > outAmount)
    {
        std::string addr;
        if (!rpc::getNewAddress(m_wallet.user, m_wallet.passwd, m_wallet.ip, m_wallet.port, addr))
        {
            // cancel transaction
            sendCancelTransaction(id, crRpcError);
            return true;
        }

        LOG() << "OUTPUTS <" << addr
              << "> amount " << (inAmount-outAmount-fee-taxToSend) / m_wallet.COIN;

        // rest
        CScript script = destination(addr);
        tx1.vout.push_back(CTxOut(inAmount-outAmount-fee-taxToSend, script));
    }

    // serialize
    std::string unsignedTx1 = txToString(tx1);
    std::string signedTx1 = unsignedTx1;

    if (!rpc::signRawTransaction(m_wallet.user, m_wallet.passwd, m_wallet.ip, m_wallet.port, signedTx1))
    {
        // do not sign, cancel
        sendCancelTransaction(id, crNotSigned);
        return true;
    }

    tx1 = txFromString(signedTx1);

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
    std::string unsignedTx2 = txToString(tx2);
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