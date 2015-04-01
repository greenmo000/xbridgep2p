//*****************************************************************************
//*****************************************************************************

#include "xbridgesession.h"
#include "xbridgeapp.h"
#include "xbridgeexchange.h"
#include "util/util.h"
#include "util/logger.h"
#include "dht/dht.h"

#include <boost/asio.hpp>
#include <boost/asio/buffer.hpp>

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
    // process this messages
    m_processors[xbcInvalid]             .bind(this, &XBridgeSession::processInvalid);

    // xchat message from wallet
    m_processors[xbcAnnounceAddresses]   .bind(this, &XBridgeSession::processAnnounceAddresses);

    // wallet received transaction
    m_processors[xbcReceivedTransaction] .bind(this, &XBridgeSession::processBitcoinTransactionHash);

    // process transaction from client wallet
    m_processors[xbcTransaction]         .bind(this, &XBridgeSession::processTransaction);
    m_processors[xbcTransactionHoldApply].bind(this, &XBridgeSession::processTransactionHoldApply);
    m_processors[xbcTransactionPayApply] .bind(this, &XBridgeSession::processTransactionPayApply);

    // retranslate messages to xbridge network
    m_processors[xbcXChatMessage]        .bind(this, &XBridgeSession::processXBridgeMessage);


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

    XBridgeApp * app = qobject_cast<XBridgeApp *>(qApp);
    app->storageClean(shared_from_this());
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
bool XBridgeSession::processPacket(XBridgePacketPtr packet)
{
    // DEBUG_TRACE();

    if (!decryptPacket(packet))
    {
        ERR() << "packet decoding error " << __FUNCTION__;
        return false;
    }

    XBridgeCommand c = packet->command();

    if (m_processors.count(c) == 0)
    {
        m_processors[xbcInvalid](packet);
        ERR() << "incorrect command code <" << c << "> " << __FUNCTION__;
        return false;
    }

    if (!m_processors[c](packet))
    {
        ERR() << "packet processing error <" << c << "> " << __FUNCTION__;
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::processInvalid(XBridgePacketPtr /*packet*/)
{
    DEBUG_TRACE();
    LOG() << "xbcInvalid command processed";
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
        ERR() << "invalid packet size for xbcAnnounceAddresses " << __FUNCTION__;
        return false;
    }

    XBridgeApp * app = qobject_cast<XBridgeApp *>(qApp);
    app->storageStore(shared_from_this(), packet->data());
    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::sendXBridgeMessage(const std::vector<unsigned char> & message)
{
    // DEBUG_TRACE();

    XBridgePacketPtr packet(new XBridgePacket());
    // packet->setData(message);
    packet->copyFrom(message);

    boost::system::error_code error;
    m_socket->send(boost::asio::buffer(packet->header(), packet->allSize()), 0, error);
    if (error)
    {
        ERR() << "packet send error " << PrintErrorCode(error) << __FUNCTION__;
        return false;
    }

    return false;
}

//*****************************************************************************
// retranslate packets from wallet to xbridge network
//*****************************************************************************
bool XBridgeSession::processXBridgeMessage(XBridgePacketPtr packet)
{
    DEBUG_TRACE();

    // size must be > 20 bytes (160bit)
    if (packet->size() <= 20)
    {
        ERR() << "invalid packet size for xbcXChatMessage " << __FUNCTION__;
        return false;
    }

    // read dest address
    std::vector<unsigned char> daddr(packet->data(), packet->data() + 20);

    XBridgeApp * app = qobject_cast<XBridgeApp *>(qApp);
    app->onSend(daddr, std::vector<unsigned char>(packet->header(), packet->header()+packet->allSize()));

    return true;
}

//*****************************************************************************
// retranslate packets from wallet to xbridge network
//*****************************************************************************
bool XBridgeSession::processXBridgeBroadcastMessage(XBridgePacketPtr packet)
{
    DEBUG_TRACE();

    XBridgeApp * app = qobject_cast<XBridgeApp *>(qApp);
    app->onSend(std::vector<unsigned char>(),
                std::vector<unsigned char>(packet->header(),
                                           packet->header()+packet->allSize()));

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::processTransaction(XBridgePacketPtr packet)
{
    DEBUG_TRACE();

    // size must be 84 bytes
    if (packet->size() != 84)
    {
        ERR() << "invalid packet size for xbcTransaction " << __FUNCTION__;
        return false;
    }

    // check and process packet if bridge is exchange
    XBridgeExchange & e = XBridgeExchange::instance();
    if (e.isEnabled())
    {
        // read packet data
        uint256 id(packet->data());

        // source
        std::vector<unsigned char> saddr(packet->data()+20, packet->data()+40);
        std::string scurrency((const char *)packet->data()+40);
        boost::uint32_t samount = *static_cast<boost::uint32_t *>(static_cast<void *>(packet->data()+48));

        // destination
        std::vector<unsigned char> daddr(packet->data()+52, packet->data()+72);
        std::string dcurrency((const char *)packet->data()+72);
        boost::uint32_t damount = *static_cast<boost::uint32_t *>(static_cast<void *>(packet->data()+80));

        LOG() << "received transaction " << util::base64_encode(std::string((char *)id.begin(), 20)) << std::endl
              << "    from" << util::base64_encode(std::string((char *)&saddr[0], 20)) << std::endl
              << "            " << scurrency << " : " << samount << std::endl
              << "    to  " << util::base64_encode(std::string((char *)&daddr[0], 20)) << std::endl
              << "            " << dcurrency << " : " << damount << std::endl;

        if (!e.haveConnectedWallet(scurrency) || !e.haveConnectedWallet(dcurrency))
        {
            LOG() << "no active wallet for transaction " << util::base64_encode(std::string((char *)id.begin(), 20));
        }
        else
        {
            // float rate = (float) destAmount / sourceAmount;
            uint256 transactionId;
            if (e.createTransaction(saddr, scurrency, samount, daddr, dcurrency, damount, transactionId))
            {
                // check transaction state, if trNew - do nothing,
                // if trJoined = send hold to client
                if (e.transaction(transactionId)->state() == XBridgeTransaction::trJoined)
                {
                    // send hold to clients
                    XBridgePacketPtr reply(new XBridgePacket(xbcTransactionHold));
                    reply->append(id.begin(), 20);
                    reply->append(transactionId.begin(), 20);

                    // TODO saddr ???
                    XBridgeApp * app = qobject_cast<XBridgeApp *>(qApp);
                    app->onSend(saddr,
                                std::vector<unsigned char>(reply->header(),
                                                           reply->header()+reply->allSize()));
                }
            }
        }
    }

    // ..and retranslate
    return processXBridgeBroadcastMessage(packet);
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::processTransactionHoldApply(XBridgePacketPtr packet)
{
    DEBUG_TRACE();

    XBridgeExchange & e = XBridgeExchange::instance();

    // TODO send hold to first client
    // TODO or send pay to both clients if hold apply from first client

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::processTransactionPayApply(XBridgePacketPtr packet)
{
    DEBUG_TRACE();

    uint256 hash;
    hash.SetHex((char *)packet->data());

    LOG() << "transaction " << hash.GetHex();

    XBridgeExchange::instance().updateTransaction(hash);
    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::processBitcoinTransactionHash(XBridgePacketPtr packet)
{
    DEBUG_TRACE();

    // size must be == 32 bytes (256bit)
//    if (packet->size() <= 32)
//    {
//        ERR() << "invalid packet size for xbcReceivedTransaction " << __FUNCTION__;
//        return false;
//    }

    uint256 hash;
    hash.SetHex((char *)packet->data());

    LOG() << "transaction " << hash.GetHex();

    // XBridgeExchange::instance().updateTransactions(hash);

    return true;
}
