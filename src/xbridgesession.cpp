//*****************************************************************************
//*****************************************************************************

// #include <boost/asio.hpp>
// #include <boost/asio/buffer.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

#include "xbridgesession.h"
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

#include "json/json_spirit.h"
#include "json/json_spirit_reader_template.h"
#include "json/json_spirit_writer_template.h"
#include "json/json_spirit_utils.h"

using namespace json_spirit;

//******************************************************************************
//******************************************************************************
// Threshold for nLockTime: below this value it is interpreted as block number,
// otherwise as UNIX timestamp.
// Tue Nov  5 00:53:20 1985 UTC
const unsigned int LOCKTIME_THRESHOLD = 500000000;

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
XBridgeSession::XBridgeSession()
{
    init();
}

//*****************************************************************************
//*****************************************************************************
XBridgeSession::XBridgeSession(const WalletParam & wallet)
    : m_wallet(wallet)
//    , m_currency(wallet)
//    , m_walletAddress(walletAddress)
//    , m_address(address)
//    , m_port(port)
//    , m_user(user)
//    , m_passwd(passwd)
//    , m_prefix(prefix)
//    , m_COIN(COIN)
//    , m_minAmount(minAmount)
{
    init();
}

//*****************************************************************************
//*****************************************************************************
XBridgeSession::~XBridgeSession()
{
}

//*****************************************************************************
//*****************************************************************************
void XBridgeSession::init()
{
    assert(!m_handlers.size());

    dht_random_bytes(m_myid, sizeof(m_myid));
    LOG() << "session <" << m_wallet.currency << "> generated id <"
          << util::base64_encode(std::string((char *)m_myid, sizeof(m_myid))).c_str()
          << ">";

    // process invalid
    m_handlers[xbcInvalid]               .bind(this, &XBridgeSession::processInvalid);

    m_handlers[xbcAnnounceAddresses]     .bind(this, &XBridgeSession::processAnnounceAddresses);

    // process transaction from client wallet
    // if (XBridgeExchange::instance().isEnabled())
    {
        m_handlers[xbcTransaction]           .bind(this, &XBridgeSession::processTransaction);
        m_handlers[xbcTransactionAccepting]  .bind(this, &XBridgeSession::processTransactionAccepting);
    }
    // else
    {
        m_handlers[xbcPendingTransaction]    .bind(this, &XBridgeSession::processPendingTransaction);
    }

    // transaction processing
    {
        m_handlers[xbcTransactionHold]       .bind(this, &XBridgeSession::processTransactionHold);
        m_handlers[xbcTransactionHoldApply]  .bind(this, &XBridgeSession::processTransactionHoldApply);

        m_handlers[xbcTransactionInit]       .bind(this, &XBridgeSession::processTransactionInit);
        m_handlers[xbcTransactionInitialized].bind(this, &XBridgeSession::processTransactionInitialized);

        m_handlers[xbcTransactionCreate]     .bind(this, &XBridgeSession::processTransactionCreate);
        m_handlers[xbcTransactionCreated]    .bind(this, &XBridgeSession::processTransactionCreated);

        m_handlers[xbcTransactionSignRefund]       .bind(this, &XBridgeSession::processTransactionSignRefund);
        m_handlers[xbcTransactionRefundSigned]     .bind(this, &XBridgeSession::processTransactionRefundSigned);

        m_handlers[xbcTransactionCommitStage1]     .bind(this, &XBridgeSession::processTransactionCommitStage1);
        m_handlers[xbcTransactionCommitedStage1]   .bind(this, &XBridgeSession::processTransactionCommitedStage1);
        m_handlers[xbcTransactionCommitStage2]     .bind(this, &XBridgeSession::processTransactionCommitStage2);
        m_handlers[xbcTransactionCommitedStage2]   .bind(this, &XBridgeSession::processTransactionCommitedStage2);

        m_handlers[xbcTransactionConfirm]    .bind(this, &XBridgeSession::processTransactionConfirm);

        m_handlers[xbcTransactionCancel]     .bind(this, &XBridgeSession::processTransactionCancel);
        m_handlers[xbcTransactionRollback]   .bind(this, &XBridgeSession::processTransactionRollback);
        m_handlers[xbcTransactionFinished]   .bind(this, &XBridgeSession::processTransactionFinished);
        m_handlers[xbcTransactionDropped]    .bind(this, &XBridgeSession::processTransactionDropped);

        m_handlers[xbcTransactionConfirmed]  .bind(this, &XBridgeSession::processTransactionConfirmed);

        // wallet received transaction
        m_handlers[xbcReceivedTransaction]   .bind(this, &XBridgeSession::processBitcoinTransactionHash);
    }

    m_handlers[xbcAddressBookEntry].bind(this, &XBridgeSession::processAddressBookEntry);

    // retranslate messages to xbridge network
    m_handlers[xbcXChatMessage].bind(this, &XBridgeSession::processXChatMessage);
}

//*****************************************************************************
//*****************************************************************************
void XBridgeSession::start(XBridge::SocketPtr socket)
{
    // DEBUG_TRACE();

    LOG() << "client connected " << socket.get();

    m_socket = socket;

    doReadHeader(XBridgePacketPtr(new XBridgePacket));
}

//*****************************************************************************
//*****************************************************************************
void XBridgeSession::disconnect()
{
    // DEBUG_TRACE();

    m_socket->close();

    LOG() << "client disconnected " << m_socket.get();

    XBridgeApp & app = XBridgeApp::instance();
    app.storageClean(shared_from_this());
}

//*****************************************************************************
//*****************************************************************************
void XBridgeSession::doReadHeader(XBridgePacketPtr packet,
                                  const std::size_t offset)
{
    // DEBUG_TRACE();

    m_socket->async_read_some(
                boost::asio::buffer(packet->header()+offset,
                                    packet->headerSize-offset),
                boost::bind(&XBridgeSession::onReadHeader,
                            shared_from_this(),
                            packet, offset,
                            boost::asio::placeholders::error,
                            boost::asio::placeholders::bytes_transferred));
}

//*****************************************************************************
//*****************************************************************************
void XBridgeSession::onReadHeader(XBridgePacketPtr packet,
                                  const std::size_t offset,
                                  const boost::system::error_code & error,
                                  std::size_t transferred)
{
    // DEBUG_TRACE();

    if (error)
    {
        ERR() << PrintErrorCode(error);
        disconnect();
        return;
    }

    if (offset + transferred != packet->headerSize)
    {
        LOG() << "partially read header, read " << transferred
              << " of " << packet->headerSize << " bytes";

        doReadHeader(packet, offset + transferred);
        return;
    }

    if (!checkXBridgePacketVersion(packet))
    {
        ERR() << "incorrect protocol version <" << packet->version() << "> " << __FUNCTION__;
        disconnect();
        return;
    }

    packet->alloc();
    doReadBody(packet);
}

//*****************************************************************************
//*****************************************************************************
void XBridgeSession::doReadBody(XBridgePacketPtr packet,
                const std::size_t offset)
{
    // DEBUG_TRACE();

    m_socket->async_read_some(
                boost::asio::buffer(packet->data()+offset,
                                    packet->size()-offset),
                boost::bind(&XBridgeSession::onReadBody,
                            shared_from_this(),
                            packet, offset,
                            boost::asio::placeholders::error,
                            boost::asio::placeholders::bytes_transferred));
}

//*****************************************************************************
//*****************************************************************************
void XBridgeSession::onReadBody(XBridgePacketPtr packet,
                                const std::size_t offset,
                                const boost::system::error_code & error,
                                std::size_t transferred = 0)
{
    // DEBUG_TRACE();

    if (error)
    {
        ERR() << PrintErrorCode(error);
        disconnect();
        return;
    }

    if (offset + transferred != packet->size())
    {
        LOG() << "partially read packet, read " << transferred
              << " of " << packet->size() << " bytes";

        doReadBody(packet, offset + transferred);
        return;
    }

    if (!processPacket(packet))
    {
        ERR() << "packet processing error " << __FUNCTION__;
    }

    doReadHeader(XBridgePacketPtr(new XBridgePacket));
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::encryptPacket(XBridgePacketPtr /*packet*/)
{
    // DEBUG_TRACE();
    // TODO implement this
    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::decryptPacket(XBridgePacketPtr /*packet*/)
{
    // DEBUG_TRACE();
    // TODO implement this
    return true;
}

//*****************************************************************************
//*****************************************************************************
void XBridgeSession::sendPacket(const std::vector<unsigned char> & to,
                                XBridgePacketPtr packet)
{
    XBridgeApp & app = XBridgeApp::instance();
    app.onSend(std::vector<unsigned char>(m_myid, m_myid + 20),
               to,
               packet->body());
}

//*****************************************************************************
//*****************************************************************************
void XBridgeSession::sendPacket(const std::string & to, XBridgePacketPtr packet)
{
    return sendPacket(rpc::toXAddr(to), packet);
}

//*****************************************************************************
// return true if packet for me and need to process
//*****************************************************************************
bool XBridgeSession::checkPacketAddress(XBridgePacketPtr packet)
{
    if (packet->size() < 20)
    {
        return false;
    }

    // check address
    if (memcmp(packet->data(), m_myid, 20) == 0)
    {
        // this session address, need to process
        return true;
    }

    for (const std::vector<unsigned char> & addr : m_addressBook)
    {
        if (addr.size() != 20)
        {
            assert(false && "incorrect address length");
            continue;
        }

        if (memcmp(packet->data(), &addr[0], 20) == 0)
        {
            // client address, need to process
            return true;
        }
    }

    // not for me
    return false;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::processPacket(XBridgePacketPtr packet)
{
    // DEBUG_TRACE();

    if (!decryptPacket(packet))
    {
        ERR() << "packet decoding error " << __FUNCTION__;
        return false;
    }

    XBridgeCommand c = packet->command();

    if (m_handlers.count(c) == 0)
    {
        m_handlers[xbcInvalid](packet);
        // ERR() << "incorrect command code <" << c << "> " << __FUNCTION__;
        return false;
    }

    if (!m_handlers[c](packet))
    {
        ERR() << "packet processing error <" << c << "> " << __FUNCTION__;
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::processInvalid(XBridgePacketPtr packet)
{
    // DEBUG_TRACE();
    LOG() << "xbcInvalid instead of " << packet->command();
    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::processZero(XBridgePacketPtr /*packet*/)
{
    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::processAnnounceAddresses(XBridgePacketPtr packet)
{
    // DEBUG_TRACE();

    // size must be 20 bytes (160bit)
    if (packet->size() != 20)
    {
        ERR() << "invalid packet size for xbcAnnounceAddresses "
              << "need 20 received " << packet->size() << " "
              << __FUNCTION__;
        return false;
    }

    XBridgeApp & app = XBridgeApp::instance();
    app.storageStore(shared_from_this(), packet->data());
    return true;
}

//*****************************************************************************
//*****************************************************************************
// static
bool XBridgeSession::checkXBridgePacketVersion(XBridgePacketPtr packet)
{
    if (packet->version() != static_cast<boost::uint32_t>(XBRIDGE_PROTOCOL_VERSION))
    {
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::sendXBridgeMessage(XBridgePacketPtr packet)
{
    boost::system::error_code error;
    m_socket->send(boost::asio::buffer(packet->header(), packet->allSize()), 0, error);
    if (error)
    {
        ERR() << "packet send error " << PrintErrorCode(error) << __FUNCTION__;
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::takeXBridgeMessage(const std::vector<unsigned char> & message)
{
    // DEBUG_TRACE();

    XBridgePacketPtr packet(new XBridgePacket());
    // packet->setData(message);
    packet->copyFrom(message);

    // return sendXBridgeMessage(packet);
    return processPacket(packet);
}

//*****************************************************************************
// retranslate packets from wallet to xbridge network
//*****************************************************************************
bool XBridgeSession::processXChatMessage(XBridgePacketPtr packet)
{
    DEBUG_TRACE();

    // size must be > 20 bytes (160bit)
    if (packet->size() <= 20)
    {
        ERR() << "invalid packet size for xbcXChatMessage "
              << "need more than 20 received " << packet->size() << " "
              << __FUNCTION__;
        return false;
    }

    // read dest address
    std::vector<unsigned char> daddr(packet->data(), packet->data() + 20);

    XBridgeApp & app = XBridgeApp::instance();
    app.onSend(std::vector<unsigned char>(m_myid, m_myid + 20),
               daddr,
               std::vector<unsigned char>(packet->header(), packet->header()+packet->allSize()));

    return true;
}

//*****************************************************************************
// retranslate packets from wallet to xbridge network
//*****************************************************************************
void XBridgeSession::sendPacketBroadcast(XBridgePacketPtr packet)
{
    // DEBUG_TRACE();

    XBridgeApp & app = XBridgeApp::instance();
    app.onSend(std::vector<unsigned char>(m_myid, m_myid+20), packet);
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::processTransaction(XBridgePacketPtr packet)
{
    // check and process packet if bridge is exchange
    XBridgeExchange & e = XBridgeExchange::instance();
    if (!e.isEnabled())
    {
        return true;
    }

    // DEBUG_TRACE();
    DEBUG_TRACE_LOG(currencyToLog());

    // size must be > 132 bytes
    if (packet->size() <= 132)
    {
        ERR() << "invalid packet size for xbcTransaction "
              << "need min 132 bytes, received " << packet->size() << " "
              << __FUNCTION__;
        return false;
    }

    // source
    uint32_t offset = 32;
    std::string saddr(reinterpret_cast<const char *>(packet->data())+offset);
    offset += saddr.size() + 1;
    std::string scurrency((const char *)packet->data()+offset);
    offset += 8;
    uint64_t samount = *static_cast<boost::uint64_t *>(static_cast<void *>(packet->data()+offset));
    offset += sizeof(uint64_t);

    // destination
    std::string daddr(reinterpret_cast<const char *>(packet->data())+offset);
    offset += daddr.size() + 1;
    std::string dcurrency((const char *)packet->data()+offset);
    offset += 8;
    boost::uint64_t damount = *static_cast<boost::uint64_t *>(static_cast<void *>(packet->data()+offset));

    // read packet data
    uint256 id(packet->data());

    LOG() << "received transaction " << util::base64_encode(std::string((char *)id.begin(), 32)) << std::endl
          << "    from " << saddr << std::endl
          << "             " << scurrency << " : " << samount << std::endl
          << "    to   " << daddr << std::endl
          << "             " << dcurrency << " : " << damount << std::endl;

    {
        uint256 pendingId;
        if (!e.createTransaction(id,
                                 saddr, scurrency, samount,
                                 daddr, dcurrency, damount,
                                 pendingId))
        {
            // not created, send cancel
            sendCancelTransaction(id, crXbridgeRejected);
        }
        else
        {
            // tx created

            // TODO send signal to gui for debug
            {
                XBridgeTransactionDescr d;
                d.id           = id;
                d.fromCurrency = scurrency;
                d.fromAmount   = samount;
                d.toCurrency   = dcurrency;
                d.toAmount     = damount;
                d.state        = XBridgeTransactionDescr::trPending;

                uiConnector.NotifyXBridgePendingTransactionReceived(d);
            }

            XBridgeTransactionPtr tr = e.pendingTransaction(pendingId);
            if (tr->id() == uint256())
            {
                LOG() << "transaction not found after create. "
                      << util::base64_encode(std::string((char *)id.begin(), 32));
                return false;
            }

            LOG() << "transaction created, id "
                  << util::base64_encode(std::string((char *)id.begin(), 32));

            boost::mutex::scoped_lock l(tr->m_lock);

            std::string firstCurrency = tr->a_currency();
            std::vector<unsigned char> fc(8, 0);
            std::copy(firstCurrency.begin(), firstCurrency.end(), fc.begin());
            std::string secondCurrency = tr->b_currency();
            std::vector<unsigned char> sc(8, 0);
            std::copy(secondCurrency.begin(), secondCurrency.end(), sc.begin());

            // broadcast send pending transaction packet
            XBridgePacketPtr reply(new XBridgePacket(xbcPendingTransaction));
            reply->append(tr->id().begin(), 32);
            reply->append(fc);
            reply->append(tr->a_amount());
            reply->append(sc);
            reply->append(tr->b_amount());
            reply->append(sessionAddr(), 20);
            reply->append(tr->tax());

            sendPacketBroadcast(reply);
        }
    }

    return true;
}

//******************************************************************************
//******************************************************************************
bool XBridgeSession::processPendingTransaction(XBridgePacketPtr packet)
{
    XBridgeExchange & e = XBridgeExchange::instance();
    if (e.isEnabled())
    {
        return true;
    }

    DEBUG_TRACE_LOG(currencyToLog());

    if (packet->size() != 88)
    {
        ERR() << "incorrect packet size for xbcPendingTransaction "
              << "need 88 received " << packet->size() << " "
              << __FUNCTION__;
        return false;
    }

    XBridgeTransactionDescrPtr ptr(new XBridgeTransactionDescr);
    ptr->id           = uint256(packet->data());
    ptr->fromCurrency = std::string(reinterpret_cast<const char *>(packet->data()+32));
    ptr->fromAmount   = *reinterpret_cast<boost::uint64_t *>(packet->data()+40);
    ptr->toCurrency   = std::string(reinterpret_cast<const char *>(packet->data()+48));
    ptr->toAmount     = *reinterpret_cast<boost::uint64_t *>(packet->data()+56);
    ptr->hubAddress   = std::vector<unsigned char>(packet->data()+64, packet->data()+84);
    ptr->tax          = *reinterpret_cast<boost::uint32_t *>(packet->data()+84);
    ptr->state        = XBridgeTransactionDescr::trPending;

    {
        boost::mutex::scoped_lock l(XBridgeApp::m_txLocker);
        if (!XBridgeApp::m_pendingTransactions.count(ptr->id))
        {
            // new transaction, copy data
            XBridgeApp::m_pendingTransactions[ptr->id] = ptr;
        }
        else
        {
            // existing, update timestamp
            XBridgeApp::m_pendingTransactions[ptr->id]->updateTimestamp(*ptr);
        }
    }

    uiConnector.NotifyXBridgePendingTransactionReceived(*ptr);

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::processTransactionAccepting(XBridgePacketPtr packet)
{
    // check and process packet if bridge is exchange
    XBridgeExchange & e = XBridgeExchange::instance();
    if (!e.isEnabled())
    {
        return true;
    }

    // DEBUG_TRACE();
    DEBUG_TRACE_LOG(currencyToLog());

    if (packet->size() <= 152)
    {
        ERR() << "invalid packet size for xbcTransactionAccepting "
              << "need min 152 bytes, received " << packet->size() << " "
              << __FUNCTION__;
        return false;
    }

    // source
    uint32_t offset = 52;
    std:: string saddr(reinterpret_cast<const char *>(packet->data()+offset));
    offset += saddr.size()+1;
    std::string scurrency((const char *)packet->data()+offset);
    offset += 8;
    uint64_t samount = *static_cast<uint64_t *>(static_cast<void *>(packet->data()+offset));
    offset += sizeof(uint64_t);

    // destination
    std::string daddr(reinterpret_cast<const char *>(packet->data()+offset));
    offset += daddr.size()+1;
    std::string dcurrency((const char *)packet->data()+offset);
    offset += 8;
    uint64_t damount = *static_cast<boost::uint64_t *>(static_cast<void *>(packet->data()+offset));
    // offset += sizeof(uint64_t);

    // read packet data
    uint256 id(packet->data());

    // TODO for debug
//    {
//        XBridgeTransactionDescr d;
//        d.id           = id;
//        d.fromCurrency = scurrency;
//        d.fromAmount   = samount;
//        d.toCurrency   = dcurrency;
//        d.toAmount     = damount;
//        d.state        = XBridgeTransactionDescr::trPending;

//        uiConnector.NotifyXBridgePendingTransactionReceived(d);
//    }

    LOG() << "received accepting transaction " << util::base64_encode(std::string((char *)id.begin(), 32)) << std::endl
          << "    from " << saddr << std::endl
          << "             " << scurrency << " : " << samount << std::endl
          << "    to   " << daddr << std::endl
          << "             " << dcurrency << " : " << damount << std::endl;

    {
        uint256 transactionId;
        if (e.acceptTransaction(id, saddr, scurrency, samount, daddr, dcurrency, damount, transactionId))
        {
            // check transaction state, if trNew - do nothing,
            // if trJoined = send hold to client
            XBridgeTransactionPtr tr = e.transaction(transactionId);

            boost::mutex::scoped_lock l(tr->m_lock);

            if (tr && tr->state() == XBridgeTransaction::trJoined)
            {
                // send hold to clients

                // first
                // TODO remove this log
                LOG() << "send xbcTransactionHold ";

                XBridgePacketPtr reply1(new XBridgePacket(xbcTransactionHold));
                reply1->append(sessionAddr(), 20);
                reply1->append(tr->id().begin(), 32);

                sendPacketBroadcast(reply1);
            }
        }
    }

    return true;
}

//******************************************************************************
//******************************************************************************
bool XBridgeSession::processTransactionHold(XBridgePacketPtr packet)
{
    DEBUG_TRACE_LOG(currencyToLog());

    if (packet->size() != 52)
    {
        ERR() << "incorrect packet size for xbcTransactionHold "
              << "need 52 received " << packet->size() << " "
              << __FUNCTION__;
        return false;
    }

    // smart hub addr
    std::vector<unsigned char> hubAddress(packet->data(), packet->data()+20);

    // read packet data
    uint256 id(packet->data()+20);

    {
        // for xchange node remove tx
        // TODO mark as finished for debug
        XBridgeExchange & e = XBridgeExchange::instance();
        if (e.isEnabled())
        {
            XBridgeTransactionPtr tr = e.transaction(id);

            boost::mutex::scoped_lock l(tr->m_lock);

            if (!tr || tr->state() != XBridgeTransaction::trJoined)
            {
                e.deletePendingTransactions(id);
                uiConnector.NotifyXBridgeTransactionStateChanged(id, XBridgeTransactionDescr::trFinished);
            }

            return true;
        }
    }

    XBridgeTransactionDescrPtr xtx;

    {
        boost::mutex::scoped_lock l(XBridgeApp::m_txLocker);

        if (!XBridgeApp::m_pendingTransactions.count(id))
        {
            // wtf? unknown transaction
            assert(!"unknown transaction");
            LOG() << "unknown transaction " << util::to_str(id) << " " << __FUNCTION__;
            return true;
        }

        if (XBridgeApp::m_transactions.count(id))
        {
            // wtf?
            assert(!"duplicate transaction");
            LOG() << "duplicate transaction " << util::to_str(id) << " " << __FUNCTION__;
            return true;
        }

        xtx = XBridgeApp::m_pendingTransactions[id];

        // remove from pending
        XBridgeApp::m_pendingTransactions.erase(id);

        if (!xtx->isLocal())
        {
            xtx->state = XBridgeTransactionDescr::trFinished;
        }
        else
        {
            // move to processing
            XBridgeApp::m_transactions[id] = xtx;

            xtx->state = XBridgeTransactionDescr::trHold;
        }
    }

    uiConnector.NotifyXBridgeTransactionStateChanged(id, xtx->state);

    if (xtx->isLocal())
    {
        // send hold apply
        XBridgePacketPtr reply(new XBridgePacket(xbcTransactionHoldApply));
        reply->append(hubAddress);
        reply->append(rpc::toXAddr(xtx->from));
        reply->append(id.begin(), 32);

        sendPacket(hubAddress, reply);
    }
    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::processTransactionHoldApply(XBridgePacketPtr packet)
{
    // DEBUG_TRACE();
    DEBUG_TRACE_LOG(currencyToLog());

    // size must be eq 72 bytes
    if (packet->size() != 72)
    {
        ERR() << "invalid packet size for xbcTransactionHoldApply "
              << "need 72 received " << packet->size() << " "
              << __FUNCTION__;
        return false;
    }

    // check is for me
    if (!checkPacketAddress(packet))
    {
        return true;
    }

    XBridgeExchange & e = XBridgeExchange::instance();
    if (!e.isEnabled())
    {
        return true;
    }

    std::vector<unsigned char> from(packet->data()+20, packet->data()+40);

    // transaction id
    uint256 id(packet->data()+40);

    XBridgeTransactionPtr tr = e.transaction(id);
    boost::mutex::scoped_lock l(tr->m_lock);

    tr->updateTimestamp();

    std::string sfrom = tr->fromXAddr(from);
    if (!sfrom.size())
    {
        ERR() << "invalid transaction address " << __FUNCTION__;
        sendCancelTransaction(id, crInvalidAddress);
        return true;
    }

    if (e.updateTransactionWhenHoldApplyReceived(tr, sfrom))
    {
        if (tr->state() == XBridgeTransaction::trHold)
        {
            // send initialize transaction command to clients

            // field length must be 8 bytes
            std::string firstCurrency = tr->a_currency();
            std::vector<unsigned char> fc(8, 0);
            std::copy(firstCurrency.begin(), firstCurrency.end(), fc.begin());
            std::string secondCurrency = tr->b_currency();
            std::vector<unsigned char> sc(8, 0);
            std::copy(secondCurrency.begin(), secondCurrency.end(), sc.begin());

            // first
            // TODO remove this log
            LOG() << "send xbcTransactionInit to "
                  << util::base64_encode(std::string((char *)&tr->a_destination()[0], 20));

            XBridgePacketPtr reply1(new XBridgePacket(xbcTransactionInit));
            reply1->append(rpc::toXAddr(tr->a_destination()));
            reply1->append(sessionAddr(), 20);
            reply1->append(id.begin(), 32);
            reply1->append(tr->a_address());
            reply1->append(fc);
            reply1->append(tr->a_amount());
            reply1->append(tr->a_destination());
            reply1->append(sc);
            reply1->append(tr->b_amount());

            sendPacket(rpc::toXAddr(tr->a_destination()), reply1);

            // second
            // TODO remove this log
            LOG() << "send xbcTransactionInit to "
                  << util::base64_encode(std::string((char *)&tr->b_destination()[0], 20));

            XBridgePacketPtr reply2(new XBridgePacket(xbcTransactionInit));
            reply2->append(rpc::toXAddr(tr->b_destination()));
            reply2->append(sessionAddr(), 20);
            reply2->append(id.begin(), 32);
            reply2->append(tr->b_address());
            reply2->append(sc);
            reply2->append(tr->b_amount());
            reply2->append(tr->b_destination());
            reply2->append(fc);
            reply2->append(tr->a_amount());

            sendPacket(rpc::toXAddr(tr->b_destination()), reply2);
        }
    }

    return true;
}

//******************************************************************************
//******************************************************************************
bool XBridgeSession::processTransactionInit(XBridgePacketPtr packet)
{
    DEBUG_TRACE_LOG(currencyToLog());

    if (packet->size() <= 172)
    {
        ERR() << "incorrect packet size for xbcTransactionInit "
              << "need 172 bytes min, received " << packet->size() << " "
              << __FUNCTION__;
        return false;
    }

    std::vector<unsigned char> thisAddress(packet->data(), packet->data()+20);
    std::vector<unsigned char> hubAddress(packet->data()+20, packet->data()+40);

    uint256 txid(packet->data()+40);

    uint32_t offset = 72;
    std::string   from(reinterpret_cast<const char *>(packet->data()+offset));
    offset += from.size()+1;
    std::string   fromCurrency(reinterpret_cast<const char *>(packet->data()+offset));
    offset += 8;
    uint64_t      fromAmount(*reinterpret_cast<boost::uint64_t *>(packet->data()+offset));
    offset += sizeof(uint64_t);

    std::string   to(reinterpret_cast<const char *>(packet->data()+offset));
    offset += to.size()+1;
    std::string   toCurrency(reinterpret_cast<const char *>(packet->data()+offset));
    offset += 8;
    uint64_t      toAmount(*reinterpret_cast<boost::uint64_t *>(packet->data()+offset));
    // offset += sizeof(uint64_t);

    // create transaction
    // without id (non this client transaction)
    XBridgeTransactionDescrPtr ptr(new XBridgeTransactionDescr);
    // ptr->id           = txid;
    ptr->from         = from;
    ptr->fromCurrency = fromCurrency;
    ptr->fromAmount   = fromAmount;
    ptr->to           = to;
    ptr->toCurrency   = toCurrency;
    ptr->toAmount     = toAmount;

    bool compressed = true;

    CKey km;
    km.MakeNewKey();

    ptr->mPubKey = km.GetPubKey();
    ptr->mSecret = km.GetSecret(compressed);

    assert(compressed && "must be compressed key");

    CKey kx;
    kx.MakeNewKey();

    ptr->xPubKey = kx.GetPubKey();
    ptr->xSecret = kx.GetSecret(compressed);

    assert(compressed && "must be compressed key");

    {
        // TODO tmp code
        {
            boost::mutex::scoped_lock l(XBridgeApp::m_txLocker);
            if (XBridgeApp::m_transactions.count(txid))
            {
                if (XBridgeApp::m_transactions[txid]->fromCurrency != ptr->fromCurrency)
                {
                    assert(!"bad init packet received");
                    return true;
                }
            }
        }

        boost::mutex::scoped_lock l(XBridgeApp::m_txHelperLocker);
        assert((XBridgeApp::m_helperTransactions.count(txid) == 0) && "transaction exists");
        XBridgeApp::m_helperTransactions[txid] = ptr;
    }

    // send initialized
    XBridgePacketPtr reply(new XBridgePacket(xbcTransactionInitialized));
    reply->append(hubAddress);
    reply->append(thisAddress);
    reply->append(txid.begin(), 32);
    reply->append(ptr->xPubKey.Raw());
    reply->append(ptr->mPubKey.Raw());

    sendPacket(hubAddress, reply);

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::processTransactionInitialized(XBridgePacketPtr packet)
{
    // DEBUG_TRACE();
    DEBUG_TRACE_LOG(currencyToLog());

    // size must be eq 138 bytes
    if (packet->size() != 138)
    {
        ERR() << "invalid packet size for xbcTransactionHoldApply "
              << "need 138 received " << packet->size() << " "
              << __FUNCTION__;
        return false;
    }

    // check is for me
    if (!checkPacketAddress(packet))
    {
        return true;
    }

    XBridgeExchange & e = XBridgeExchange::instance();
    if (!e.isEnabled())
    {
        return true;
    }

    std::vector<unsigned char> from(packet->data()+20, packet->data()+40);

    // transaction id
    uint256 id(packet->data()+40);

    CPubKey x  (packet->data()+72, packet->data()+105);
    CPubKey pk1(packet->data()+105, packet->data()+138);

    XBridgeTransactionPtr tr = e.transaction(id);
    boost::mutex::scoped_lock l(tr->m_lock);

    tr->updateTimestamp();

    std::string sfrom = tr->fromXAddr(from);
    if (!sfrom.size())
    {
        ERR() << "invalid transaction address " << __FUNCTION__;
        sendCancelTransaction(id, crInvalidAddress);
        return true;
    }

    if (e.updateTransactionWhenInitializedReceived(tr, sfrom, x, pk1))
    {
        if (tr->state() == XBridgeTransaction::trInitialized)
        {
            // send create transaction command to clients

            // first
            // TODO remove this log
            LOG() << "send xbcTransactionCreate to "
                  << util::base64_encode(std::string((char *)&tr->a_address()[0], 20));

            // send xbcTransactionCreate
            // with nLockTime == lockTime*2 for first client,
            // with nLockTime == lockTime*4 for second
            XBridgePacketPtr reply1(new XBridgePacket(xbcTransactionCreate));
            reply1->append(rpc::toXAddr(tr->a_address()));
            reply1->append(sessionAddr(), 20);
            reply1->append(id.begin(), 32);
            reply1->append(tr->b_destination());
            reply1->append(tr->a_taxAddress());
            reply1->append(static_cast<uint32_t>(0));
            reply1->append(static_cast<uint16_t>('A'));
            reply1->append(tr->b_x().Raw());
            reply1->append(tr->b_pk1().Raw());

            sendPacket(tr->a_address(), reply1);

            // second
            // TODO remove this log
            LOG() << "send xbcTransactionCreate to "
                  << util::base64_encode(std::string((char *)&tr->b_address()[0], 20));

            XBridgePacketPtr reply2(new XBridgePacket(xbcTransactionCreate));
            reply2->append(rpc::toXAddr(tr->b_address()));
            reply2->append(sessionAddr(), 20);
            reply2->append(id.begin(), 32);
            reply2->append(tr->a_destination());
            reply2->append(tr->b_taxAddress());
            reply2->append(tr->tax());
            reply2->append(static_cast<uint16_t>('B'));
            reply2->append(tr->b_x().Raw());
            reply2->append(tr->a_pk1().Raw());

            sendPacket(tr->b_address(), reply2);
        }
    }

    return true;
}

//******************************************************************************
//******************************************************************************
//CScript destination(const std::vector<unsigned char> & address, const char prefix)
//{
//    uint160 uikey(address);
//    CKeyID key(uikey);

//    CBitcoinAddress baddr;
//    baddr.Set(key, prefix);

//    CScript addr;
//    addr.SetDestination(baddr.Get());
//    return addr;
//}

////******************************************************************************
////******************************************************************************
//CScript destination(const std::string & address)
//{
//    CBitcoinAddress baddr(address);

//    CScript addr;
//    addr.SetDestination(baddr.Get());
//    return addr;
//}

//******************************************************************************
//******************************************************************************
//std::string txToStringBTC(const CBTCTransaction & tx)
//{
//    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
//    ss << tx;
//    return HexStr(ss.begin(), ss.end());
//}

//******************************************************************************
//******************************************************************************
//std::string txToString(const CTransaction & tx)
//{
//    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
//    ss << tx;
//    return HexStr(ss.begin(), ss.end());
//}

//******************************************************************************
//******************************************************************************
//CBTCTransaction txFromStringBTC(const std::string & str)
//{
//    CBTCTransaction tx;

//    try
//    {
//        std::vector<unsigned char> txdata = ParseHex(str);
//        CDataStream stream(txdata,
//                           SER_NETWORK, PROTOCOL_VERSION);
//        stream >> tx;
//    }
//    catch (std::exception & e)
//    {
//        LOG() << "exception " << e.what() << " in " << __FUNCTION__;
//    }
//    catch (...)
//    {
//        LOG() << "unknown exception in " << __FUNCTION__;
//    }

//    return tx;
//}

//******************************************************************************
//******************************************************************************
//CTransaction txFromString(const std::string & str)
//{
//    CTransaction tx;
//    try
//    {
//        std::vector<unsigned char> txdata = ParseHex(str);
//        CDataStream stream(txdata,
//                           SER_NETWORK, PROTOCOL_VERSION);
//        stream >> tx;
//    }
//    catch (std::exception & e)
//    {
//        LOG() << "exception " << e.what() << " in " << __FUNCTION__;
//    }
//    catch (...)
//    {
//        LOG() << "unknown exception in " << __FUNCTION__;
//    }
//    return tx;
//}

//******************************************************************************
//******************************************************************************
boost::uint64_t XBridgeSession::minTxFee(const uint32_t inputCount, const uint32_t outputCount)
{
    return 148*inputCount + 34*outputCount + 10;
}

//******************************************************************************
//******************************************************************************
std::string XBridgeSession::round_x(const long double val, uint32_t prec)
{
    long double value = val;
    value *= pow(10, prec);

    value += .5;
    value = std::floor(value);

    std::string svalue = boost::lexical_cast<std::string>(value); // std::to_string(value);

    if (prec >= svalue.length())
    {
        svalue.insert(0, prec-svalue.length()+1, '0');
    }
    svalue.insert(svalue.length()-prec, 1, '.');

    value = std::stold(svalue);
//    return value;
    return svalue;
}

//******************************************************************************
//******************************************************************************
bool XBridgeSession::processTransactionCreate(XBridgePacketPtr packet)
{
    DEBUG_TRACE_LOG(currencyToLog());

    if (packet->size() < 212)
    {
        ERR() << "incorrect packet size for xbcTransactionCreate "
              << "need min 212 bytes, received " << packet->size() << " "
              << __FUNCTION__;
        return false;
    }

    std::vector<unsigned char> thisAddress(packet->data(), packet->data()+20);
    std::vector<unsigned char> hubAddress(packet->data()+20, packet->data()+40);

    // transaction id
    uint256 txid(packet->data()+40);

    // destination address
    uint32_t offset = 72;
    std::string destAddress(reinterpret_cast<const char *>(packet->data()+offset));
    offset += destAddress.size()+1;

    // tax
    std::string taxAddress(reinterpret_cast<const char *>(packet->data()+offset));
    offset += taxAddress.size()+1;

    const uint32_t taxPercent = *reinterpret_cast<uint32_t *>(packet->data()+offset);
    offset += sizeof(uint32_t);

    const char role = static_cast<char>((*reinterpret_cast<uint16_t *>(packet->data()+offset)));
    offset += sizeof(uint16_t);

    CPubKey x(packet->data()+offset, packet->data()+offset+33);
    offset += 33;

    CPubKey mPubkey(packet->data()+offset, packet->data()+offset+33);
    // offset += 33;

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

    xtx->role = role;

    std::vector<rpc::Unspent> entries;
    if (!rpc::listUnspent(m_wallet.user, m_wallet.passwd,
                          m_wallet.ip, m_wallet.port, entries))
    {
        LOG() << "rpc::listUnspent failed" << __FUNCTION__;
        return true;
    }

    double outAmount = static_cast<double>(xtx->fromAmount)/XBridgeTransactionDescr::COIN;
    double taxToSend = outAmount*(taxPercent/100000);

    double fee1      = 0;
    double fee2      = static_cast<double>(minTxFee(2, 1))/XBridgeTransactionDescr::COIN;
    double inAmount  = 0;

    std::vector<rpc::Unspent> usedInTx;
    for (const rpc::Unspent & entry : entries)
    {
        usedInTx.push_back(entry);
        inAmount += entry.amount;
        fee1 = static_cast<double>(minTxFee(usedInTx.size(), taxToSend > 0 ? 4 : 3)) / XBridgeTransactionDescr::COIN;

        LOG() << "USED FOR TX <" << entry.txId << "> amount " << entry.amount << " " << entry.vout << " fee " << fee1;

        // check amount
        if (inAmount >= outAmount+fee1+fee2+taxToSend)
        {
            break;
        }
    }

    // check amount
    if (inAmount < outAmount+fee1+fee2+taxToSend)
    {
        // no money, cancel transaction
        LOG() << "no money, transaction canceled " << __FUNCTION__;
        sendCancelTransaction(txid, crNoMoney);
        return true;
    }

    // create transactions

    // create multisig address for first tx
    {
        bool compressed = true;
        CKey km;
        km.MakeNewKey();
        xtx->mPubKey = km.GetPubKey();
        xtx->mSecret = km.GetSecret(compressed);
        assert(compressed && "must be compressed key");

        std::vector<CKey> pubkeys;
        pubkeys.resize(2);
        pubkeys[0].SetPubKey(mPubkey);
        pubkeys[1].SetPubKey(xtx->mPubKey);

        CScript inner;
        inner.SetMultisig(2, pubkeys);
        CBitcoinAddress baddr;
        baddr.Set(inner.GetID(), m_wallet.scriptPrefix[0]);

        xtx->multisig = baddr.ToString();
        xtx->redeem   = HexStr(inner.begin(), inner.end());

    } // multisig

    // binTx
    std::string binjson;
    {
        std::vector<std::pair<std::string, int> >    inputs;
        std::vector<std::pair<std::string, double> > outputs;

        // inputs
        for (const rpc::Unspent & entry : usedInTx)
        {
            inputs.push_back(std::make_pair(entry.txId, entry.vout));
        }

        // outputs

        // amount
        outputs.push_back(std::make_pair(xtx->multisig, outAmount));

        // fee2 to x
        CBitcoinAddress baddr;
        baddr.Set(x.GetID(), m_wallet.addrPrefix[0]);

        outputs.push_back(std::make_pair(baddr.ToString(), fee2));

        // tax
        if (taxToSend)
        {
            outputs.push_back(std::make_pair(taxAddress, taxToSend));
        }

        // rest
        if (inAmount > outAmount+fee1+fee2+taxToSend)
        {
            std::string addr;
            if (!rpc::getNewAddress(m_wallet.user, m_wallet.passwd,
                                    m_wallet.ip, m_wallet.port,
                                    addr))
            {
                // cancel transaction
                LOG() << "rpc error, transaction canceled " << __FUNCTION__;
                sendCancelTransaction(txid, crRpcError);
                return true;
            }

            double rest = inAmount-outAmount-fee1-fee2-taxToSend;
            outputs.push_back(std::make_pair(addr, rest));
        }

        std::string bintx;
        if (!rpc::createRawTransaction(m_wallet.user, m_wallet.passwd,
                                       m_wallet.ip, m_wallet.port,
                                       inputs, outputs, 0, bintx))
        {
            // cancel transaction
            LOG() << "create transaction error, transaction canceled " << __FUNCTION__;
            sendCancelTransaction(txid, crRpcError);
            return true;
        }

        // sign
        bool complete = false;
        if (!rpc::signRawTransaction(m_wallet.user, m_wallet.passwd,
                                     m_wallet.ip, m_wallet.port,
                                     bintx, complete))
        {
            // do not sign, cancel
            LOG() << "sign transaction error, transaction canceled " << __FUNCTION__;
            sendCancelTransaction(txid, crNotSigned);
            return true;
        }

        assert(complete && "not fully signed");

        std::string bintxid;
        if (!rpc::decodeRawTransaction(m_wallet.user, m_wallet.passwd,
                                       m_wallet.ip, m_wallet.port,
                                       bintx, bintxid, binjson))
        {
            LOG() << "decode signed transaction error, transaction canceled " << __FUNCTION__;
            sendCancelTransaction(txid, crRpcError);
            return true;
        }

        TXLOG() << "bailin  " << bintx;
        TXLOG() << binjson;

        xtx->binTx   = bintx;
        xtx->binTxId = bintxid;

    } // binTx

    // prevtxs for sign next transactions
    {
        std::vector<std::tuple<std::string, int, std::string, std::string> > prevtxs;

        Value v;
        if (!read_string(binjson, v))
        {
            // wtf ?
            ERR() << "error read tx json, cancelled " << binjson;
            sendCancelTransaction(txid, crRpcError);
            return true;
        }

        Object o = v.get_obj();
        v = find_value(o, "vout");
        Array outs = v.get_array();
        for (const Value & out : outs)
        {
            Value vn = find_value(out.get_obj(), "n");
            if (vn.type() != int_type)
            {
                continue;
            }
            uint32_t n = vn.get_int();

            Value vscript = find_value(out.get_obj(), "scriptPubKey");
            if (vscript.type() != obj_type)
            {
                continue;
            }

            Object script = vscript.get_obj();

            Value vhex = find_value(script, "hex");
            if (vhex.type() != str_type)
            {
                continue;
            }

            prevtxs.push_back(std::make_tuple(xtx->binTxId, n, vhex.get_str(), n == 0 ? xtx->redeem : ""));
        }

        xtx->prevtxs = rpc::prevtxsJson(prevtxs);
    } // inputs

    // payTx
    {
        std::vector<std::pair<std::string, int> >    inputs;
        std::vector<std::pair<std::string, double> > outputs;

        // inputs from binTx
        inputs.push_back(std::make_pair(xtx->binTxId, 0));
        inputs.push_back(std::make_pair(xtx->binTxId, 1));

        // outputs
        outputs.push_back(std::make_pair(destAddress, outAmount));

        std::string paytx;
        if (!rpc::createRawTransaction(m_wallet.user, m_wallet.passwd,
                                       m_wallet.ip, m_wallet.port,
                                       inputs, outputs, 0, paytx))
        {
            // cancel transaction
            LOG() << "create transaction error, transaction canceled " << __FUNCTION__;
            sendCancelTransaction(txid, crRpcError);
            return true;
        }

        // sign
        std::vector<std::string> keys;
        keys.push_back(secretToString(xtx->mSecret));

        bool complete = false;
        if (!rpc::signRawTransaction(m_wallet.user, m_wallet.passwd,
                                     m_wallet.ip, m_wallet.port,
                                     paytx, xtx->prevtxs, keys, complete))
        {
            // do not sign, cancel
            LOG() << "sign transaction error, transaction canceled " << __FUNCTION__;
            sendCancelTransaction(txid, crNotSigned);
            return true;
        }

        assert(!complete && "not fully signed");

        std::string json;
        std::string paytxid;
        if (!rpc::decodeRawTransaction(m_wallet.user, m_wallet.passwd,
                                       m_wallet.ip, m_wallet.port,
                                       paytx, paytxid, json))
        {
            LOG() << "decode signed transaction error, transaction canceled " << __FUNCTION__;
            sendCancelTransaction(txid, crRpcError);
            return true;
        }

        TXLOG() << "payment  " << paytx;
        TXLOG() << json;

        xtx->payTx   = paytx;
        xtx->payTxId = paytxid;

    } // payTx

    // refTx
    {
        std::vector<std::pair<std::string, int> >    inputs;
        std::vector<std::pair<std::string, double> > outputs;

        // inputs from binTx
        inputs.push_back(std::make_pair(xtx->binTxId, 0));

        // outputs
        {
            std::string addr;
            if (!rpc::getNewAddress(m_wallet.user, m_wallet.passwd, m_wallet.ip, m_wallet.port, addr))
            {
                // cancel transaction
                LOG() << "rpc error, transaction canceled " << __FUNCTION__;
                sendCancelTransaction(txid, crRpcError);
                return true;
            }

            double fee3 = static_cast<double>(minTxFee(1, 1)) / XBridgeTransactionDescr::COIN;

            outputs.push_back(std::make_pair(addr, outAmount-fee3));
        }

        // lock time
        uint32_t lockTime;
        time_t local = time(0); // GetAdjustedTime();
        if (role == 'A')
        {
            lockTime = local + 600; // 259200; // 72h in seconds
        }
        else if (role == 'B')
        {
            lockTime = local + 300; // 259200/2; // 36h in seconds
        }

        std::string reftx;
        if (!rpc::createRawTransaction(m_wallet.user, m_wallet.passwd,
                                       m_wallet.ip, m_wallet.port,
                                       inputs, outputs, 0/*lockTime*/, reftx))
        {
            // cancel transaction
            LOG() << "create transaction error, transaction canceled " << __FUNCTION__;
            sendCancelTransaction(txid, crRpcError);
            return true;
        }

        // sign
        std::vector<std::string> keys;
        keys.push_back(secretToString(xtx->mSecret));

        bool complete = false;
        if (!rpc::signRawTransaction(m_wallet.user, m_wallet.passwd,
                                     m_wallet.ip, m_wallet.port,
                                     reftx, xtx->prevtxs, keys, complete))
        {
            // do not sign, cancel
            LOG() << "sign transaction error, transaction canceled " << __FUNCTION__;
            sendCancelTransaction(txid, crNotSigned);
            return true;
        }

        assert(!complete && "not fully signed");

        std::string json;
        std::string reftxid;
        if (!rpc::decodeRawTransaction(m_wallet.user, m_wallet.passwd,
                                       m_wallet.ip, m_wallet.port,
                                       reftx, reftxid, json))
        {
            LOG() << "decode signed transaction error, transaction canceled " << __FUNCTION__;
            sendCancelTransaction(txid, crRpcError);
            return true;
        }

        TXLOG() << "refund  " << reftx;
        TXLOG() << json;

        xtx->refTx   = reftx;
        xtx->refTxId = reftxid;

    } // refTx

    xtx->state = XBridgeTransactionDescr::trCreated;
    uiConnector.NotifyXBridgeTransactionStateChanged(txid, xtx->state);

    // send reply
    XBridgePacketPtr reply(new XBridgePacket(xbcTransactionCreated));
    reply->append(hubAddress);
    reply->append(thisAddress);
    reply->append(txid.begin(), 32);
    reply->append(xtx->prevtxs);
    reply->append(xtx->refTx);

    sendPacket(hubAddress, reply);
    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::processTransactionCreated(XBridgePacketPtr packet)
{
    // DEBUG_TRACE();
    DEBUG_TRACE_LOG(currencyToLog());

    // size must be > 74 bytes
    if (packet->size() < 74)
    {
        ERR() << "invalid packet size for xbcTransactionCreated "
              << "need more than 74 received " << packet->size() << " "
              << __FUNCTION__;
        return false;
    }

    // check is for me
    if (!checkPacketAddress(packet))
    {
        return true;
    }

    XBridgeExchange & e = XBridgeExchange::instance();
    if (!e.isEnabled())
    {
        return true;
    }

    size_t offset = 20;

    std::vector<unsigned char> from(packet->data()+offset, packet->data()+offset+20);
    offset += 20;

    uint256 txid(packet->data()+offset);
    offset += 32;

    std::string prevtxs(reinterpret_cast<const char *>(packet->data()+offset));
    offset += prevtxs.size()+1;

    std::string refTx(reinterpret_cast<const char *>(packet->data()+offset));
    // offset += refTx.size()+1;

    XBridgeTransactionPtr tr = e.transaction(txid);
    boost::mutex::scoped_lock l(tr->m_lock);

    tr->updateTimestamp();

    std::string sfrom = tr->fromXAddr(from);
    if (!sfrom.size())
    {
        ERR() << "invalid transaction address " << __FUNCTION__;
        sendCancelTransaction(txid, crInvalidAddress);
        return true;
    }

    if (e.updateTransactionWhenCreatedReceived(tr, sfrom, prevtxs, refTx))
    {
        if (tr->state() == XBridgeTransaction::trCreated)
        {
            // send packets for sign transaction

            // TODO remove this log
            LOG() << "send xbcTransactionSign to "
                  << util::base64_encode(std::string((char *)&tr->a_destination()[0], 20));

            XBridgePacketPtr reply(new XBridgePacket(xbcTransactionSignRefund));
            reply->append(rpc::toXAddr(tr->a_destination()));
            reply->append(sessionAddr(), 20);
            reply->append(txid.begin(), 32);
            reply->append(tr->b_prevtxs());
            reply->append(tr->b_refTx());

            sendPacket(tr->a_destination(), reply);

            // TODO remove this log
            LOG() << "send xbcTransactionSign to "
                  << util::base64_encode(std::string((char *)&tr->b_destination()[0], 20));

            XBridgePacketPtr reply2(new XBridgePacket(xbcTransactionSignRefund));
            reply2->append(rpc::toXAddr(tr->b_destination()));
            reply2->append(sessionAddr(), 20);
            reply2->append(txid.begin(), 32);
            reply2->append(tr->a_prevtxs());
            reply2->append(tr->a_refTx());

            sendPacket(tr->b_destination(), reply2);
        }
    }

    return true;
}

//******************************************************************************
//******************************************************************************
bool XBridgeSession::processTransactionSignRefund(XBridgePacketPtr packet)
{
    DEBUG_TRACE_LOG(currencyToLog());

    if (packet->size() < 74)
    {
        ERR() << "incorrect packet size for xbcTransactionSign "
              << "need more than 74 received " << packet->size() << " "
              << __FUNCTION__;
        return false;
    }

    std::vector<unsigned char> thisAddress(packet->data(), packet->data()+20);

    size_t offset = 20;
    std::vector<unsigned char> hubAddress(packet->data()+offset, packet->data()+offset+20);
    offset += 20;

    uint256 txid(packet->data()+offset);
    offset += 32;

    std::string prevtxs(reinterpret_cast<const char *>(packet->data()+offset));
    offset += prevtxs.size()+1;

    std::string reftx(reinterpret_cast<const char *>(packet->data()+offset));
    // offset += reftx.size()+1;

    // check txid
    XBridgeTransactionDescrPtr xtx;
    {
        boost::mutex::scoped_lock l(XBridgeApp::m_txHelperLocker);

        if (!XBridgeApp::m_helperTransactions.count(txid))
        {
            // wtf? unknown transaction
            LOG() << "unknown transaction " << util::to_str(txid) << " " << __FUNCTION__;
            return true;
        }

        xtx = XBridgeApp::m_helperTransactions[txid];
    }

    // TODO check txpay/txref, inputs/outputs/locktime, use decorerawtransaction
    // unserialize
//    {
//        CTransaction txpay = txFromString(payTx);
//        CTransaction txref = txFromString(refTx);

//        if (txref.nLockTime < LOCKTIME_THRESHOLD)
//        {
//            // not signed, cancel tx
//            sendCancelTransaction(txid, crNotSigned);
//            return true;
//        }
//    }

    std::vector<std::string> keys;
    keys.push_back(secretToString(xtx->mSecret));

    TXLOG() << "refund unsigned " << reftx;

    // sign refund tx
    bool complete = false;
    if (!rpc::signRawTransaction(m_wallet.user, m_wallet.passwd, m_wallet.ip, m_wallet.port,
                                 reftx, prevtxs, keys, complete))
    {
        // do not sign, cancel
        sendCancelTransaction(txid, crNotSigned);
        return true;
    }

    assert(complete && "not fully signed");

    TXLOG() << "refund   signed " << reftx;

//#ifdef __DEBUG__

//    std::string json;
//    std::string reftxid;
//    if (rpc::decodeRawTransaction(m_wallet.user, m_wallet.passwd,
//                                   m_wallet.ip, m_wallet.port,
//                                   reftx, reftxid, json))
//    {
//        TXLOG() << json;
//    }

//#endif

    // sign payment tx
//    complete = false;
//    if (!rpc::signRawTransaction(m_wallet.user, m_wallet.passwd, m_wallet.ip, m_wallet.port,
//                                 paytx, prevtxs, keys, complete))
//    {
//        // do not sign, cancel
//        sendCancelTransaction(txid, crNotSigned);
//        return true;
//    }

//    assert(!complete && "not fully signed");

    xtx->state = XBridgeTransactionDescr::trSigned;
    uiConnector.NotifyXBridgeTransactionStateChanged(txid, xtx->state);

    // send reply
    XBridgePacketPtr reply(new XBridgePacket(xbcTransactionRefundSigned));
    reply->append(hubAddress);
    reply->append(thisAddress);
    reply->append(txid.begin(), 32);
    // reply->append(paytx);
    reply->append(reftx);

    sendPacket(hubAddress, reply);
    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::processTransactionRefundSigned(XBridgePacketPtr packet)
{
    // DEBUG_TRACE();
    DEBUG_TRACE_LOG(currencyToLog());

    // size must be > 72 bytes
    if (packet->size() < 72)
    {
        ERR() << "invalid packet size for xbcTransactionSigned "
              << "need more than 72 received " << packet->size() << " "
              << __FUNCTION__;
        return false;
    }

    // check is for me
    if (!checkPacketAddress(packet))
    {
        return true;
    }

    XBridgeExchange & e = XBridgeExchange::instance();
    if (!e.isEnabled())
    {
        return true;
    }

    std::vector<unsigned char> from(packet->data()+20, packet->data()+40);
    uint256 txid(packet->data()+40);

    size_t offset = 72;
//    std::string payTx(reinterpret_cast<const char *>(packet->data()+offset));
//    offset += payTx.size()+1;
    std::string refTx(reinterpret_cast<const char *>(packet->data()+offset));
    offset += refTx.size()+1;

    XBridgeTransactionPtr tr = e.transaction(txid);
    boost::mutex::scoped_lock l(tr->m_lock);

    tr->updateTimestamp();

    std::string sfrom = tr->fromXAddr(from);
    if (!sfrom.size())
    {
        ERR() << "invalid transaction address " << __FUNCTION__;
        sendCancelTransaction(txid, crInvalidAddress);
        return true;
    }

    if (e.updateTransactionWhenSignedReceived(tr, sfrom, refTx))
    {
        if (tr->state() == XBridgeTransaction::trSigned)
        {
            // send signed transactions to clients

            // TODO remove this log
            LOG() << "send xbcTransactionCommitStage1 to "
                  << util::base64_encode(std::string((char *)&tr->a_address()[0], 20));

            XBridgePacketPtr reply(new XBridgePacket(xbcTransactionCommitStage1));
            reply->append(rpc::toXAddr(tr->a_address()));
            reply->append(sessionAddr(), 20);
            reply->append(txid.begin(), 32);
            reply->append(tr->a_refTx());

            sendPacket(tr->a_address(), reply);

            // TODO remove this log
            LOG() << "send xbcTransactionCommitStage1 to "
                  << util::base64_encode(std::string((char *)&tr->b_address()[0], 20));

            XBridgePacketPtr reply2(new XBridgePacket(xbcTransactionCommitStage1));
            reply2->append(rpc::toXAddr(tr->b_address()));
            reply2->append(sessionAddr(), 20);
            reply2->append(txid.begin(), 32);
            reply2->append(tr->b_refTx());

            sendPacket(tr->b_address(), reply2);
        }
    }

    return true;
}

//******************************************************************************
//******************************************************************************
bool XBridgeSession::processTransactionCommitStage1(XBridgePacketPtr packet)
{
    DEBUG_TRACE_LOG(currencyToLog());

    if (packet->size() < 72)
    {
        ERR() << "incorrect packet size for xbcTransactionCommit "
              << "need more than 72 received " << packet->size() << " "
              << __FUNCTION__;
        return false;
    }

    std::vector<unsigned char> thisAddress(packet->data(), packet->data()+20);
    std::vector<unsigned char> hubAddress(packet->data()+20, packet->data()+40);

    uint256 txid(packet->data()+40);

    size_t offset = 72;
//    std::string payTx(reinterpret_cast<const char *>(packet->data()+offset));
//    offset += payTx.size()+1;
    std::string refTx(reinterpret_cast<const char *>(packet->data()+offset));
    offset += refTx.size()+1;

    // send pay transaction to network
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

    // refund tx
    // TODO check signature etc.
    xtx->refTx = refTx;

    if (!rpc::sendRawTransaction(m_wallet.user, m_wallet.passwd, m_wallet.ip, m_wallet.port, xtx->binTx))
    {
        // not commited....send cancel???
        // sendCancelTransaction(id, crNotAccepted);
        LOG() << "bin tx not commited " << util::to_str(txid) << " " << __FUNCTION__;
        return true;
    }

    if (!rpc::sendRawTransaction(m_wallet.user, m_wallet.passwd, m_wallet.ip, m_wallet.port, xtx->refTx))
    {
        // not commited....send cancel???
        // sendCancelTransaction(id, crNotAccepted);
        LOG() << "ref tx not commited " << util::to_str(txid) << " " << __FUNCTION__;
        return true;
    }

//    uint256 walletTxId = (static_cast<CTransaction *>(&xtx->payTx))->GetHash();
//    m_mapWalletTxToXBridgeTx[walletTxId] = txid;

    xtx->state = XBridgeTransactionDescr::trCommited;
    uiConnector.NotifyXBridgeTransactionStateChanged(txid, xtx->state);

    assert(!"not finished");
    return true;

    // send commit apply to hub
    XBridgePacketPtr reply(new XBridgePacket(xbcTransactionCommitedStage1));
    reply->append(hubAddress);
    reply->append(thisAddress);
    reply->append(txid.begin(), 32);
    reply->append(xtx->binTxId);

    sendPacket(hubAddress, reply);
    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::processTransactionCommitedStage1(XBridgePacketPtr packet)
{
    // DEBUG_TRACE();
    DEBUG_TRACE_LOG(currencyToLog());

    // size must be > 72 bytes
    if (packet->size() <= 72)
    {
        ERR() << "invalid packet size for xbcTransactionCommited "
              << "need bin 72 bytes, received " << packet->size() << " "
              << __FUNCTION__;
        return false;
    }

    // check is for me
    if (!checkPacketAddress(packet))
    {
        return true;
    }

    XBridgeExchange & e = XBridgeExchange::instance();
    if (!e.isEnabled())
    {
        return true;
    }

    std::vector<unsigned char> from(packet->data()+20, packet->data()+40);
    uint256 txid(packet->data()+40);
    std::string bintxid(reinterpret_cast<const char *>(packet->data()+72));

    XBridgeTransactionPtr tr = e.transaction(txid);
    boost::mutex::scoped_lock l(tr->m_lock);

    tr->updateTimestamp();

    std::string sfrom = tr->fromXAddr(from);
    if (!sfrom.size())
    {
        ERR() << "invalid transaction address " << __FUNCTION__;
        sendCancelTransaction(txid, crInvalidAddress);
        return true;
    }

    e.updateTransactionWhenCommitedReceived(tr, sfrom, bintxid);

    if (tr->state() == XBridgeTransaction::trSigned)
    {
        // send commit transactions to clients

        // TODO remove this log
        LOG() << "send xbcTransactionCommitStage2 to "
              << util::base64_encode(std::string((char *)&tr->b_address()[0], 20));

        XBridgePacketPtr reply2(new XBridgePacket(xbcTransactionCommitStage2));
        reply2->append(tr->b_address());
        reply2->append(sessionAddr(), 20);
        reply2->append(txid.begin(), 32);
        reply2->append(tr->b_payTx());
        reply2->append(tr->b_refTx());

        sendPacket(tr->b_address(), reply2);
    }
}

//******************************************************************************
//******************************************************************************
bool XBridgeSession::processTransactionCommitStage2(XBridgePacketPtr packet)
{
    DEBUG_TRACE_LOG(currencyToLog());

    if (packet->size() < 72)
    {
        ERR() << "incorrect packet size for xbcTransactionCommit "
              << "need more than 72 received " << packet->size() << " "
              << __FUNCTION__;
        return false;
    }

    std::vector<unsigned char> thisAddress(packet->data(), packet->data()+20);
    std::vector<unsigned char> hubAddress(packet->data()+20, packet->data()+40);

    uint256 txid(packet->data()+40);


    size_t offset = 72;
    std::string payTx(reinterpret_cast<const char *>(packet->data()+offset));
    offset += payTx.size()+1;
    std::string refTx(reinterpret_cast<const char *>(packet->data()+offset));

    // send pay transaction to network
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

    // unserialize signed transaction
    // CTransaction txrev = txFromString(rawtx);
    xtx->payTx = payTx;
    xtx->refTx = refTx;

    assert(!"unfinished");

    // TODO send binTx before payTx
    if (!rpc::sendRawTransaction(m_wallet.user, m_wallet.passwd, m_wallet.ip, m_wallet.port, xtx->payTx))
    {
        // not commited....send cancel???
        // sendCancelTransaction(id, crNotAccepted);
        LOG() << "transaction not commited " << util::to_str(txid) << " " << __FUNCTION__;
        return true;
    }

//    uint256 walletTxId = (static_cast<CTransaction *>(&xtx->payTx))->GetHash();
//    m_mapWalletTxToXBridgeTx[walletTxId] = txid;

    xtx->state = XBridgeTransactionDescr::trCommited;
    uiConnector.NotifyXBridgeTransactionStateChanged(txid, xtx->state);

    assert(!"not finished");

    // send commit apply to hub
    XBridgePacketPtr reply(new XBridgePacket(xbcTransactionCommitedStage1));
    reply->append(hubAddress);
    reply->append(thisAddress);
    reply->append(txid.begin(), 32);
    // reply->append(xtx->payTxId.begin(), 32);

    sendPacket(hubAddress, reply);
    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::processTransactionCommitedStage2(XBridgePacketPtr packet)
{
    assert(!"not implemented");
    return true;

//    // DEBUG_TRACE();
//    DEBUG_TRACE_LOG(currencyToLog());

//    // size must be == 104 bytes
//    if (packet->size() != 104)
//    {
//        ERR() << "invalid packet size for xbcTransactionCommited "
//              << "need 104 received " << packet->size() << " "
//              << __FUNCTION__;
//        return false;
//    }

//    // check is for me
//    if (!checkPacketAddress(packet))
//    {
//        return true;
//    }

//    XBridgeExchange & e = XBridgeExchange::instance();
//    if (!e.isEnabled())
//    {
//        return true;
//    }

//    std::vector<unsigned char> from(packet->data()+20, packet->data()+40);
//    uint256 txid(packet->data()+40);
//    uint256 txhash(packet->data()+72);

//    XBridgeTransactionPtr tr = e.transaction(txid);
//    boost::mutex::scoped_lock l(tr->m_lock);

//    tr->updateTimestamp();

//    std::string sfrom = tr->fromXAddr(from);
//    if (!sfrom.size())
//    {
//        ERR() << "invalid transaction address " << __FUNCTION__;
//        sendCancelTransaction(txid, crInvalidAddress);
//        return true;
//    }

//    if (e.updateTransactionWhenCommitedReceived(tr, sfrom, txhash))
//    {
//        if (tr->state() == XBridgeTransaction::trCommited)
//        {
//            // transaction commited, wait for confirm
//            LOG() << "commit transaction, wait for confirm. id <"
//                  << txid.GetHex() << ">" << std::endl
//                  << "    hash <" << tr->a_txHash().GetHex() << ">" << std::endl
//                  << "    hash <" << tr->b_txHash().GetHex() << ">";

//            // send confirm request to clients

////            // TODO remove this log
////            LOG() << "send xbcTransactionCommit to "
////                  << util::base64_encode(std::string((char *)&tr->firstDestination()[0], 20));

//            XBridgePacketPtr reply(new XBridgePacket(xbcTransactionConfirm));
//            reply->append(tr->a_destination());
//            reply->append(sessionAddr(), 20);
//            reply->append(txid.begin(), 32);
//            reply->append(tr->b_txHash().begin(), 32);

//            sendPacket(tr->a_destination(), reply);

////            // TODO remove this log
////            LOG() << "send xbcTransactionCommit to "
////                  << util::base64_encode(std::string((char *)&tr->secondDestination()[0], 20));

//            XBridgePacketPtr reply2(new XBridgePacket(xbcTransactionConfirm));
//            reply2->append(tr->b_destination());
//            reply2->append(sessionAddr(), 20);
//            reply2->append(txid.begin(), 32);
//            reply2->append(tr->a_txHash().begin(), 32);

//            sendPacket(tr->b_destination(), reply2);
//        }
//    }

//    return true;
}

//******************************************************************************
//******************************************************************************
bool XBridgeSession::processTransactionConfirm(XBridgePacketPtr packet)
{
    DEBUG_TRACE_LOG(currencyToLog());

    if (packet->size() < 72)
    {
        LOG() << "incorrect packet size for xbcTransactionConfirm "
              << "need more than 72 received " << packet->size() << " "
              << __FUNCTION__;
        return false;
    }

    std::vector<unsigned char> thisAddress(packet->data(), packet->data()+20);
    std::vector<unsigned char> hubAddress(packet->data()+20, packet->data()+40);

    uint256 txid(packet->data()+40);
    uint256 txhash(packet->data()+72);

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

    assert(!"not finished");

    // xtx->payTxId        = txhash;
    xtx->hubAddress     = hubAddress;
    xtx->confirmAddress = thisAddress;

    // add to unconfirmed list
    {
        boost::mutex::scoped_lock l(XBridgeApp::m_txUnconfirmedLocker);
        XBridgeApp::m_unconfirmed[txid] = xtx;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::processTransactionConfirmed(XBridgePacketPtr packet)
{
    DEBUG_TRACE_LOG(currencyToLog());

    // size must be == 72 bytes
    if (packet->size() != 72)
    {
        ERR() << "invalid packet size for xbcTransactionCommited "
              << "need 72 received " << packet->size() << " "
              << __FUNCTION__;
        return false;
    }

    // check is for me
    if (!checkPacketAddress(packet))
    {
        return true;
    }

    XBridgeExchange & e = XBridgeExchange::instance();

    std::vector<unsigned char> from(packet->data()+20, packet->data()+40);
    uint256 txid(packet->data()+40);

    XBridgeTransactionPtr tr = e.transaction(txid);
    boost::mutex::scoped_lock l(tr->m_lock);

    tr->updateTimestamp();

    std::string sfrom = tr->fromXAddr(from);
    if (!sfrom.size())
    {
        ERR() << "invalid transaction address " << __FUNCTION__;
        sendCancelTransaction(txid, crInvalidAddress);
        return true;
    }

    if (e.updateTransactionWhenConfirmedReceived(tr, sfrom))
    {
        if (tr->state() == XBridgeTransaction::trFinished)
        {
            LOG() << "broadcast send xbcTransactionFinished";

            XBridgePacketPtr reply(new XBridgePacket(xbcTransactionFinished));
            reply->append(txid.begin(), 32);
            sendPacketBroadcast(reply);

            // send transaction state to clients
//            std::vector<std::vector<unsigned char> > rcpts;
//            rcpts.push_back(tr->firstAddress());
//            rcpts.push_back(tr->firstDestination());
//            rcpts.push_back(tr->secondAddress());
//            rcpts.push_back(tr->secondDestination());

//            foreach (const std::vector<unsigned char> & to, rcpts)
//            {
//                // TODO remove this log
//                LOG() << "send xbcTransactionFinished to "
//                      << util::base64_encode(std::string((char *)&to[0], 20));

//                XBridgePacketPtr reply(new XBridgePacket(xbcTransactionFinished));
//                reply->append(to);
//                reply->append(txid.begin(), 32);

//                sendPacket(to, reply);
//            }
        }
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::processTransactionCancel(XBridgePacketPtr packet)
{
    // DEBUG_TRACE();

    // size must be == 36 bytes
    if (packet->size() != 36)
    {
        ERR() << "invalid packet size for xbcReceivedTransaction "
              << "need 36 received " << packet->size() << " "
              << __FUNCTION__;
        return false;
    }

    uint256 txid(packet->data());
    TxCancelReason reason = static_cast<TxCancelReason>(*reinterpret_cast<uint32_t*>(packet->data() + 32));

    // check and process packet if bridge is exchange
    XBridgeExchange & e = XBridgeExchange::instance();
    if (e.isEnabled())
    {
        e.deletePendingTransactions(txid);
    }

    XBridgeTransactionDescrPtr xtx;
    {
        boost::mutex::scoped_lock l(XBridgeApp::m_txLocker);

        if (!XBridgeApp::m_transactions.count(txid))
        {
            LOG() << "unknown transaction " << util::to_str(txid) << " " << __FUNCTION__;
            return true;
        }

        xtx = XBridgeApp::m_transactions[txid];
    }

    // update transaction state for gui
    xtx->state = XBridgeTransactionDescr::trCancelled;
    uiConnector.NotifyXBridgeTransactionCancelled(txid, XBridgeTransactionDescr::trCancelled, reason);

    // ..and retranslate
    // sendPacketBroadcast(packet);
    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::finishTransaction(XBridgeTransactionPtr tr)
{
    LOG() << "finish transaction <" << tr->id().GetHex() << ">";

    if (tr->state() != XBridgeTransaction::trConfirmed)
    {
        ERR() << "finished unconfirmed transaction <" << tr->id().GetHex() << ">";
        return false;
    }

//    std::vector<std::vector<unsigned char> > rcpts;
//    rcpts.push_back(tr->firstAddress());
//    rcpts.push_back(tr->firstDestination());
//    rcpts.push_back(tr->secondAddress());
//    rcpts.push_back(tr->secondDestination());

//    foreach (const std::vector<unsigned char> & to, rcpts)
    {
        // TODO remove this log
//        LOG() << "send xbcTransactionFinished to "
//              << util::base64_encode(std::string((char *)&to[0], 20));

        XBridgePacketPtr reply(new XBridgePacket(xbcTransactionFinished));
        // reply->append(to);
        reply->append(tr->id().begin(), 32);

        // sendPacket(to, reply);
        sendPacketBroadcast(reply);
    }

    tr->finish();

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::sendCancelTransaction(const uint256 & txid,
                                           const TxCancelReason & reason)
{
    LOG() << "cancel transaction <" << txid.GetHex() << ">";

    // TODO remove this log
    // LOG() << "send xbcTransactionCancel to "
    //       << util::base64_encode(std::string((char *)&to[0], 20));

    XBridgePacketPtr reply(new XBridgePacket(xbcTransactionCancel));
    reply->append(txid.begin(), 32);
    reply->append(static_cast<uint32_t>(reason));

    // sendPacket(to, reply);
    sendPacketBroadcast(reply);

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::rollbackTransaction(XBridgeTransactionPtr tr)
{
    LOG() << "rollback transaction <" << tr->id().GetHex() << ">";

    std::vector<std::string> rcpts;

    if (tr->state() >= XBridgeTransaction::trSigned)
    {
        rcpts.push_back(tr->a_address());
        rcpts.push_back(tr->b_address());
    }

    for (const std::string & to : rcpts)
    {
        // TODO remove this log
        LOG() << "send xbcTransactionRollback to "
              << util::base64_encode(std::string((char *)&to[0], 20));

        XBridgePacketPtr reply(new XBridgePacket(xbcTransactionRollback));
        reply->append(to);
        reply->append(tr->id().begin(), 32);

        sendPacket(to, reply);
    }

    sendCancelTransaction(tr->id(), crRollback);
    tr->finish();

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::processBitcoinTransactionHash(XBridgePacketPtr packet)
{
    // DEBUG_TRACE();

    // size must be == 32 bytes (256bit)
    if (packet->size() != 32)
    {
        ERR() << "invalid packet size for xbcReceivedTransaction "
              << "need 32 received " << packet->size() << " "
              << __FUNCTION__;
        return false;
    }

    static XBridgeExchange & e = XBridgeExchange::instance();
    if (!e.isEnabled())
    {
        return true;
    }

    uint256 id(packet->data());
//    // LOG() << "received transaction <" << id.GetHex() << ">";

    e.updateTransaction(id);

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::processAddressBookEntry(XBridgePacketPtr packet)
{
    // DEBUG_TRACE();

    std::string currency(reinterpret_cast<const char *>(packet->data()));
    std::string name(reinterpret_cast<const char *>(packet->data()+currency.length()+1));
    std::string address(reinterpret_cast<const char *>(packet->data()+currency.length()+name.length()+2));

    XBridgeApp::instance().storeAddressBookEntry(currency, name, address);

    return true;
}

//*****************************************************************************
//*****************************************************************************
void XBridgeSession::sendListOfWallets()
{
    XBridgeExchange & e = XBridgeExchange::instance();
    if (!e.isEnabled())
    {
        return;
    }

    std::vector<StringPair> wallets = e.listOfWallets();
    std::vector<std::string> list;
    for (std::vector<StringPair>::iterator i = wallets.begin(); i != wallets.end(); ++i)
    {
        list.push_back(i->first + '|' + i->second);
    }

    XBridgePacketPtr packet(new XBridgePacket(xbcExchangeWallets));
    packet->setData(boost::algorithm::join(list, "|"));

    sendPacket(std::vector<unsigned char>(), packet);
}

//*****************************************************************************
//*****************************************************************************
void XBridgeSession::sendListOfTransactions()
{
    XBridgeApp & app = XBridgeApp::instance();

    // send my trx
    if (XBridgeApp::m_pendingTransactions.size())
    {
        if (XBridgeApp::m_txLocker.try_lock())
        {
            // send pending transactions
            for (std::map<uint256, XBridgeTransactionDescrPtr>::iterator i = XBridgeApp::m_pendingTransactions.begin();
                 i != XBridgeApp::m_pendingTransactions.end(); ++i)
            {
                app.sendPendingTransaction(i->second);
            }

            XBridgeApp::m_txLocker.unlock();
        }
    }

    // send exchange trx
    XBridgeExchange & e = XBridgeExchange::instance();
    if (!e.isEnabled())
    {
        return;
    }

    std::list<XBridgeTransactionPtr> list = e.pendingTransactions();
    std::list<XBridgeTransactionPtr>::iterator i = list.begin();
    for (; i != list.end(); ++i)
    {
        XBridgeTransactionPtr & ptr = *i;

        boost::mutex::scoped_lock l(ptr->m_lock);

        XBridgePacketPtr packet(new XBridgePacket(xbcPendingTransaction));

        // field length must be 8 bytes
        std::vector<unsigned char> fc(8, 0);
        std::string tmp = ptr->a_currency();
        std::copy(tmp.begin(), tmp.end(), fc.begin());

        // field length must be 8 bytes
        std::vector<unsigned char> tc(8, 0);
        tmp = ptr->b_currency();
        std::copy(tmp.begin(), tmp.end(), tc.begin());

        packet->append(ptr->id().begin(), 32);
        // packet->append(ptr->firstAddress());
        packet->append(fc);
        packet->append(ptr->a_amount());
        // packet->append(ptr->firstDestination());
        packet->append(tc);
        packet->append(ptr->b_amount());
        // packet->append(static_cast<boost::uint32_t>(ptr->state()));

        packet->append(sessionAddr(), 20);
        packet->append(ptr->tax());

        sendPacket(std::vector<unsigned char>(), packet);
    }
}

//*****************************************************************************
//*****************************************************************************
void XBridgeSession::eraseExpiredPendingTransactions()
{
    XBridgeExchange & e = XBridgeExchange::instance();
    if (!e.isEnabled())
    {
        return;
    }

    std::list<XBridgeTransactionPtr> list = e.pendingTransactions();
    std::list<XBridgeTransactionPtr>::iterator i = list.begin();
    for (; i != list.end(); ++i)
    {
        XBridgeTransactionPtr & ptr = *i;

        boost::mutex::scoped_lock l(ptr->m_lock);

        if (ptr->isExpired())
        {
            LOG() << "transaction expired <" << ptr->id().GetHex() << ">";
            e.deletePendingTransactions(ptr->hash1());
        }
    }
}

//*****************************************************************************
//*****************************************************************************
void XBridgeSession::checkUnconfirmedTx()
{
    XBridgeApp::instance().checkUnconfirmedTx();
}

//*****************************************************************************
//*****************************************************************************
void XBridgeSession::requestUnconfirmedTx()
{
    DEBUG_TRACE_LOG(currencyToLog());

    std::map<uint256, XBridgeTransactionDescrPtr> utx;
    {
        boost::mutex::scoped_lock l(XBridgeApp::m_txUnconfirmedLocker);
        utx = XBridgeApp::m_unconfirmed;
    }

    for (std::map<uint256, XBridgeTransactionDescrPtr>::iterator i = utx.begin(); i != utx.end(); ++i)
    {
        // TODO debug fn rpc::getTransaction, payTxId is string instead uint256

        LOG() << "check transaction " << i->second->payTxId;
        if (rpc::getTransaction(m_wallet.user, m_wallet.passwd, m_wallet.ip, m_wallet.port,
                                i->second->payTxId))
        {
            {
                boost::mutex::scoped_lock l(XBridgeApp::m_txUnconfirmedLocker);
                XBridgeApp::m_unconfirmed.erase(i->first);
            }

            XBridgeTransactionDescrPtr & tx = i->second;

            XBridgePacketPtr ptr(new XBridgePacket(xbcTransactionConfirmed));
            ptr->append(tx->hubAddress);
            ptr->append(tx->confirmAddress);
            ptr->append(i->first.begin(), 32);

            sendPacket(tx->hubAddress, ptr);
        }
    }
}

//*****************************************************************************
//*****************************************************************************
void XBridgeSession::checkFinishedTransactions()
{
    XBridgeExchange & e = XBridgeExchange::instance();
    if (!e.isEnabled())
    {
        return;
    }

    std::list<XBridgeTransactionPtr> list = e.finishedTransactions();
    std::list<XBridgeTransactionPtr>::iterator i = list.begin();
    for (; i != list.end(); ++i)
    {
        XBridgeTransactionPtr & ptr = *i;

        boost::mutex::scoped_lock l(ptr->m_lock);

        uint256 txid = ptr->id();

        if (ptr->state() == XBridgeTransaction::trConfirmed)
        {
            // send finished
            LOG() << "confirmed transaction <" << txid.GetHex() << ">";
            finishTransaction(ptr);
        }
        else if (ptr->state() == XBridgeTransaction::trCancelled)
        {
            // drop cancelled tx
            LOG() << "drop cancelled transaction <" << txid.GetHex() << ">";
            ptr->drop();
        }
        else if (ptr->state() == XBridgeTransaction::trFinished)
        {
            // delete finished tx
            LOG() << "delete finished transaction <" << txid.GetHex() << ">";
            e.deleteTransaction(txid);
        }
        else if (ptr->state() == XBridgeTransaction::trDropped)
        {
            // delete dropped tx
            LOG() << "delete dropped transaction <" << txid.GetHex() << ">";
            e.deleteTransaction(txid);
        }
        else if (!ptr->isValid())
        {
            // delete invalid tx
            LOG() << "delete invalid transaction <" << txid.GetHex() << ">";
            e.deleteTransaction(txid);
        }
        else
        {
            LOG() << "timeout transaction <" << txid.GetHex() << ">"
                  << " state " << ptr->strState();

            // send rollback
            rollbackTransaction(ptr);
        }
    }
}

//*****************************************************************************
//*****************************************************************************
void XBridgeSession::resendAddressBook()
{
    XBridgeApp::instance().resendAddressBook();
}

//*****************************************************************************
//*****************************************************************************
void XBridgeSession::getAddressBook()
{
    XBridgeApp::instance().getAddressBook();
}

//*****************************************************************************
//*****************************************************************************
void XBridgeSession::requestAddressBook()
{
    // no address book for exchange node
    XBridgeExchange & e = XBridgeExchange::instance();
    if (e.isEnabled())
    {
        return;
    }

    std::vector<rpc::AddressBookEntry> entries;
    if (!rpc::requestAddressBook(m_wallet.user, m_wallet.passwd, m_wallet.ip, m_wallet.port, entries))
    {
        return;
    }

    XBridgeApp & app = XBridgeApp::instance();

    for (const rpc::AddressBookEntry & e : entries)
    {
        for (const std::string & addr : e.second)
        {
            std::vector<unsigned char> vaddr = rpc::toXAddr(addr);
            m_addressBook.insert(vaddr);
            app.storageStore(shared_from_this(), &vaddr[0]);

            uiConnector.NotifyXBridgeAddressBookEntryReceived
                    (m_wallet.currency, e.first, addr);
        }
    }
}

//*****************************************************************************
//*****************************************************************************
void XBridgeSession::sendAddressbookEntry(const std::string & currency,
                                          const std::string & name,
                                          const std::string & address)
{
    if (m_socket->is_open())
    {
        XBridgePacketPtr p(new XBridgePacket(xbcAddressBookEntry));
        p->append(currency);
        p->append(name);
        p->append(address);

        sendXBridgeMessage(p);
    }
}

//******************************************************************************
//******************************************************************************
bool XBridgeSession::processTransactionFinished(XBridgePacketPtr packet)
{
    DEBUG_TRACE_LOG(currencyToLog());

    if (packet->size() != 32)
    {
        ERR() << "incorrect packet size for xbcTransactionFinished" << __FUNCTION__;
        return false;
    }

    // transaction id
    uint256 txid(packet->data());

    XBridgeTransactionDescrPtr xtx;
    {
        boost::mutex::scoped_lock l(XBridgeApp::m_txLocker);

        if (!XBridgeApp::m_transactions.count(txid))
        {
            // signal for gui
            uiConnector.NotifyXBridgeTransactionStateChanged(txid, XBridgeTransactionDescr::trFinished);
            return true;
        }

        xtx = XBridgeApp::m_transactions[txid];
    }

    // update transaction state for gui
    xtx->state = XBridgeTransactionDescr::trFinished;
    uiConnector.NotifyXBridgeTransactionStateChanged(txid, xtx->state);

    return true;
}

//******************************************************************************
//******************************************************************************
bool XBridgeSession::revertXBridgeTransaction(const uint256 & id)
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
        LOG() << "unknown transaction " << util::to_str(id) << " " << __FUNCTION__;
        return true;
    }

    // rollback, commit revert transaction
    if (!rpc::sendRawTransaction(m_wallet.user, m_wallet.passwd, m_wallet.ip, m_wallet.port, xtx->refTx))
    {
        // not commited....send cancel???
        // sendCancelTransaction(id);
        return true;
    }

    return true;
}

//******************************************************************************
//******************************************************************************
bool XBridgeSession::processTransactionRollback(XBridgePacketPtr packet)
{
    DEBUG_TRACE_LOG(currencyToLog());

    if (packet->size() != 52)
    {
        ERR() << "incorrect packet size for xbcTransactionRollback" << __FUNCTION__;
        return false;
    }

    // transaction id
    uint256 txid(packet->data()+20);

    // for rollback need local transaction id
    // TODO maybe hub id?
    XBridgeTransactionDescrPtr xtx;
    {
        boost::mutex::scoped_lock l(XBridgeApp::m_txLocker);

        if (!XBridgeApp::m_transactions.count(txid))
        {
            // wtf? unknown tx
            LOG() << "unknown transaction " << util::to_str(txid) << " " << __FUNCTION__;
            return true;
        }

        xtx = XBridgeApp::m_transactions[txid];
    }

    revertXBridgeTransaction(xtx->id);

    // update transaction state for gui
    xtx->state = XBridgeTransactionDescr::trRollback;
    uiConnector.NotifyXBridgeTransactionStateChanged(txid, xtx->state);

    return true;
}

//******************************************************************************
//******************************************************************************
bool XBridgeSession::processTransactionDropped(XBridgePacketPtr packet)
{
    if (packet->size() != 32)
    {
        ERR() << "incorrect packet size for xbcTransactionDropped" << __FUNCTION__;
        return false;
    }

    // transaction id
    uint256 id(packet->data());

    XBridgeTransactionDescrPtr xtx;
    {
        boost::mutex::scoped_lock l(XBridgeApp::m_txLocker);

        if (!XBridgeApp::m_transactions.count(id))
        {
            // signal for gui
            uiConnector.NotifyXBridgeTransactionStateChanged(id, XBridgeTransactionDescr::trDropped);
            return false;
        }

        xtx = XBridgeApp::m_transactions[id];
    }

    // update transaction state for gui
    xtx->state = XBridgeTransactionDescr::trDropped;
    uiConnector.NotifyXBridgeTransactionStateChanged(id, xtx->state);

    return true;
}

//******************************************************************************
//******************************************************************************
bool XBridgeSession::makeNewPubKey(CPubKey & newPKey) const
{
    if (m_wallet.isGetNewPubKeySupported)
    {
        // use getnewpubkey
        std::string key;
        if (!rpc::getNewPubKey(m_wallet.user, m_wallet.passwd,
                              m_wallet.ip, m_wallet.port, key))
        {
            return false;
        }

        newPKey = CPubKey(ParseHex(key));
    }
    else
    {
        // use importprivkey
        CKey newKey;
        newKey.MakeNewKey();
        bool compressed;
        CSecret s = newKey.GetSecret(compressed);

        assert(compressed && "must be compressed key");

        if (!rpc::importPrivKey(m_wallet.user, m_wallet.passwd,
                                m_wallet.ip, m_wallet.port,
                                secretToString(s), "",
                                m_wallet.isImportWithNoScanSupported))
        {
            return false;
        }

        newPKey = newKey.GetPubKey();
    }
    return newPKey.IsValid();
}

//******************************************************************************
//******************************************************************************
std::string XBridgeSession::secretToString(CSecret & secret) const
{
    std::vector<unsigned char> vch(secret.begin(), secret.end());
    vch.insert(vch.begin(), m_wallet.secretPrefix[0]);
    vch.push_back(1);
    return EncodeBase58Check(vch);
}
