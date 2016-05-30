//*****************************************************************************
//*****************************************************************************

// #include <boost/asio.hpp>
// #include <boost/asio/buffer.hpp>
#include <boost/algorithm/string.hpp>

#include "xbridgesessionetherium.h"
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

//******************************************************************************
//******************************************************************************
// Threshold for nLockTime: below this value it is interpreted as block number,
// otherwise as UNIX timestamp.
// Tue Nov  5 00:53:20 1985 UTC
static const unsigned int LOCKTIME_THRESHOLD = 500000000;

//******************************************************************************
//******************************************************************************
struct PrintErrorCode
{
    const boost::system::error_code & error;

    explicit PrintErrorCode(const boost::system::error_code & e) : error(e) {}

    friend std::ostream & operator<<(std::ostream & out, const PrintErrorCode & e)
    {
        return out << " ERROR <" << e.error.value() << "> " << e.error.message();
    }
};

//*****************************************************************************
//*****************************************************************************
// std::string txToString(const CTransaction & tx);
// CTransaction txFromString(const std::string & str);

//*****************************************************************************
//*****************************************************************************
XBridgeSessionEtherium::XBridgeSessionEtherium()
{
    init();
}

//*****************************************************************************
//*****************************************************************************
XBridgeSessionEtherium::XBridgeSessionEtherium(const std::string & currency,
                                               const std::string & walletAddress,
                                               const std::string & address,
                                               const std::string & port,
                                               const std::string & user,
                                               const std::string & passwd,
                                               const std::string & prefix,
                                               const boost::uint64_t & /*COIN*/,
                                               const boost::uint64_t & minAmount)
    : XBridgeSession(currency, walletAddress, address, port, user, passwd,
                     prefix, 1000000000000000000, minAmount)
{
    init();
}

//*****************************************************************************
//*****************************************************************************
XBridgeSessionEtherium::~XBridgeSessionEtherium()
{

}

//*****************************************************************************
//*****************************************************************************
void XBridgeSessionEtherium::init()
{
    assert(!m_processors.size());

    dht_random_bytes(m_myid, sizeof(m_myid));
    LOG() << "session <" << m_currency << "> generated id <"
             << util::base64_encode(std::string((char *)m_myid, sizeof(m_myid))).c_str()
             << ">";

    // process invalid
    m_processors[xbcInvalid]               .bind(this, &XBridgeSessionEtherium::processInvalid);

    m_processors[xbcAnnounceAddresses]     .bind(this, &XBridgeSessionEtherium::processAnnounceAddresses);

    // process transaction from client wallet
    // if (XBridgeExchange::instance().isEnabled())
    {
        m_processors[xbcTransaction]           .bind(this, &XBridgeSessionEtherium::processTransaction);
        m_processors[xbcTransactionAccepting]   .bind(this, &XBridgeSessionEtherium::processTransactionAccepting);
    }
    // else
    {
        m_processors[xbcPendingTransaction]    .bind(this, &XBridgeSessionEtherium::processPendingTransaction);
    }

    // transaction processing
    {
        m_processors[xbcTransactionHold]       .bind(this, &XBridgeSessionEtherium::processTransactionHold);
        m_processors[xbcTransactionHoldApply]  .bind(this, &XBridgeSessionEtherium::processTransactionHoldApply);

        m_processors[xbcTransactionInit]       .bind(this, &XBridgeSessionEtherium::processTransactionInit);
        m_processors[xbcTransactionInitialized].bind(this, &XBridgeSessionEtherium::processTransactionInitialized);

        m_processors[xbcTransactionCreate]     .bind(this, &XBridgeSessionEtherium::processTransactionCreate);
        m_processors[xbcTransactionCreated]    .bind(this, &XBridgeSessionEtherium::processTransactionCreated);

        m_processors[xbcTransactionSign]       .bind(this, &XBridgeSessionEtherium::processTransactionSign);
        m_processors[xbcTransactionSigned]     .bind(this, &XBridgeSessionEtherium::processTransactionSigned);

        m_processors[xbcTransactionCommit]     .bind(this, &XBridgeSessionEtherium::processTransactionCommit);
        m_processors[xbcTransactionCommited]   .bind(this, &XBridgeSessionEtherium::processTransactionCommited);

        m_processors[xbcTransactionConfirm]    .bind(this, &XBridgeSessionEtherium::processTransactionConfirm);

        m_processors[xbcTransactionCancel]     .bind(this, &XBridgeSessionEtherium::processTransactionCancel);
        m_processors[xbcTransactionRollback]   .bind(this, &XBridgeSessionEtherium::processTransactionRollback);
        m_processors[xbcTransactionFinished]   .bind(this, &XBridgeSessionEtherium::processTransactionFinished);
        m_processors[xbcTransactionDropped]    .bind(this, &XBridgeSessionEtherium::processTransactionDropped);

        m_processors[xbcTransactionConfirmed]  .bind(this, &XBridgeSessionEtherium::processTransactionConfirmed);

        // wallet received transaction
        m_processors[xbcReceivedTransaction]   .bind(this, &XBridgeSessionEtherium::processBitcoinTransactionHash);
    }

    m_processors[xbcAddressBookEntry].bind(this, &XBridgeSessionEtherium::processAddressBookEntry);

    // retranslate messages to xbridge network
    m_processors[xbcXChatMessage].bind(this, &XBridgeSessionEtherium::processXChatMessage);
}

//******************************************************************************
//******************************************************************************
bool XBridgeSessionEtherium::processTransactionCreate(XBridgePacketPtr packet)
{
    DEBUG_TRACE_LOG(currencyToLog());

    if (packet->size() != 124)
    {
        ERR() << "incorrect packet size for xbcTransactionCreate" << __FUNCTION__;
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
    const uint32_t tax = *reinterpret_cast<uint32_t*>(packet->data()+120);

    XBridgeTransactionDescrPtr xtx;
    {
        boost::mutex::scoped_lock l(XBridgeApp::m_txLocker);

        if (!XBridgeApp::m_transactions.count(id))
        {
            // wtf? unknown transaction
            // TODO log
            return false;
        }

        xtx = XBridgeApp::m_transactions[id];
    }

    // lock time
    xtx->lockTimeTx1 = *reinterpret_cast<boost::uint32_t *>(packet->data()+92);
    xtx->lockTimeTx2 = *reinterpret_cast<boost::uint32_t *>(packet->data()+96);

    uint64_t amount = 0;
    if (!rpc::eth_getBalance(m_address, m_port, m_walletAddress, amount))
    {
        LOG() << "rpc::eth_getBalance failed" << __FUNCTION__;
        return false;
    }

//    uint64_t gasPrice = 0;
//    if (!rpc::eth_gasPrice(m_address, m_port, gasPrice))
//    {
//        LOG() << "rpc::eth_getBalance failed" << __FUNCTION__;
//        return false;
//    }

    boost::uint64_t outAmount = m_COIN*(static_cast<double>(xtx->fromAmount)/XBridgeTransactionDescr::COIN)+m_fee;
    boost::uint64_t taxAmount = m_COIN*(static_cast<double>(xtx->fromAmount*tax/100000)/XBridgeTransactionDescr::COIN);

    boost::uint64_t fee = 0;
    boost::uint64_t inAmount  = 0;

    // check amount
    if (amount < outAmount)
    {
        // no money, cancel transaction
        sendCancelTransaction(id);
        return false;
    }

    xtx->state = XBridgeTransactionDescr::trCreated;
    uiConnector.NotifyXBridgeTransactionStateChanged(id, xtx->state);

    // send reply
    XBridgePacketPtr reply(new XBridgePacket(xbcTransactionCreated));
    reply->append(hubAddress);
    reply->append(thisAddress);
    reply->append(id.begin(), 32);
    reply->append("ETHEREUM_TRANSACTION");
    reply->append("ETHEREUM_TRANSACTION");

    sendPacket(hubAddress, reply);
    return true;
}

//******************************************************************************
//******************************************************************************
bool XBridgeSessionEtherium::processTransactionSign(XBridgePacketPtr packet)
{
    DEBUG_TRACE_LOG(currencyToLog());

    if (packet->size() < 72)
    {
        ERR() << "incorrect packet size for xbcTransactionSign" << __FUNCTION__;
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
            // TODO log
            return false;
        }

        xtx = XBridgeApp::m_transactions[txid];
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

//******************************************************************************
//******************************************************************************
bool XBridgeSessionEtherium::processTransactionCommit(XBridgePacketPtr packet)
{
    DEBUG_TRACE_LOG(currencyToLog());

    if (packet->size() < 72)
    {
        ERR() << "incorrect packet size for xbcTransactionCommit" << __FUNCTION__;
        return false;
    }

    std::vector<unsigned char> thisAddress(packet->data(), packet->data()+20);
    std::vector<unsigned char> hubAddress(packet->data()+20, packet->data()+40);

    uint256 txid(packet->data()+40);

    // std::string rawtx(reinterpret_cast<const char *>(packet->data()+72));

    // send pay transaction to network
    XBridgeTransactionDescrPtr xtx;
    {
        boost::mutex::scoped_lock l(XBridgeApp::m_txLocker);

        if (!XBridgeApp::m_transactions.count(txid))
        {
            // wtf? unknown transaction
            // TODO log
            return false;
        }

        xtx = XBridgeApp::m_transactions[txid];
    }

    std::string from = "0x" + std::string(reinterpret_cast<char *>(&xtx->from[0]), 20);
    std::string to   = "0x" + std::string(reinterpret_cast<char *>(&xtx->to[0]), 20);

    uint64_t outAmount = m_COIN*(static_cast<double>(xtx->fromAmount)/XBridgeTransactionDescr::COIN);
    if (!rpc::eth_sendTransaction(m_address, m_port, from, to, outAmount, m_fee))
    {
        // not commited....send cancel???
        // sendCancelTransaction(id);
        return false;
    }

    xtx->state = XBridgeTransactionDescr::trCommited;
    uiConnector.NotifyXBridgeTransactionStateChanged(txid, xtx->state);

    // send commit apply to hub
    XBridgePacketPtr reply(new XBridgePacket(xbcTransactionCommited));
    reply->append(hubAddress);
    reply->append(thisAddress);
    reply->append(txid.begin(), 32);
    reply->append(xtx->payTxId.begin(), 32);

    sendPacket(hubAddress, reply);
    return true;
}

//*****************************************************************************
//*****************************************************************************
std::vector<unsigned char> toXBridgeAddr(const std::string & etherAddress)
{
    std::string waddr = etherAddress.substr(2);

    assert(etherAddress.size() == 42 || "incorrect address length");
    assert(etherAddress[0] == '0' || etherAddress[1] == 'x' || "incorrect address prefix");
    assert(waddr.size() == 40 || "incorrect address length");

    std::vector<unsigned char> ucaddr;

    if (waddr.size() == 40)
    {
        char ch[4];
        memset(ch, 0, 4);

        for (size_t i = 0; i < waddr.size(); i += 2)
        {
            ch[0] = waddr[i];
            ch[1] = waddr[i+1];
            int a = strtol(ch, 0, 16);
            ucaddr.push_back(a);
        }
    }

    return ucaddr;
}

//*****************************************************************************
//*****************************************************************************
void XBridgeSessionEtherium::requestAddressBook()
{
//    std::vector<std::string> addrs;
//    if (!rpc::eth_accounts(m_address, m_port, addrs))
//    {
//        return;
//    }

//    XBridgeApp & app = XBridgeApp::instance();

//    for (const std::string & addr : addrs)
//    {
//        app.storageStore(shared_from_this(), reinterpret_cast<const unsigned char *>(addr.substr(2).c_str()));

//        uiConnector.NotifyXBridgeAddressBookEntryReceived
//                (m_currency, "ETHEREUM", util::base64_encode(addr.substr(2)));
//    }

    std::vector<unsigned char> addr = toXBridgeAddr(m_walletAddress);

    XBridgeApp & app = XBridgeApp::instance();
    app.storageStore(shared_from_this(), &addr[0]);

    uiConnector.NotifyXBridgeAddressBookEntryReceived
            (m_currency, "ETHEREUM", util::base64_encode(addr));
}

//******************************************************************************
//******************************************************************************
bool XBridgeSessionEtherium::revertXBridgeTransaction(const uint256 & id)
{
    DEBUG_TRACE_LOG(currencyToLog());

    // TODO temporary implementation
    XBridgeTransactionDescrPtr xtx;
    {
        boost::mutex::scoped_lock l(XBridgeApp::m_txLocker);

        // search tx
        for (std::map<uint256, XBridgeTransactionDescrPtr>::iterator i = XBridgeApp::m_transactions.begin();
             i != XBridgeApp::m_transactions.end(); ++i)
        {
            if (i->second->id == id)
            {
                xtx = i->second;
                break;
            }
        }
    }

    if (!xtx)
    {
        return false;
    }

//    // rollback, commit revert transaction
//    if (!rpc::sendRawTransaction(m_user, m_passwd, m_address, m_port, xtx->revTx))
//    {
//        // not commited....send cancel???
//        // sendCancelTransaction(id);
//        return false;
//    }

    return true;
}
