//******************************************************************************
//******************************************************************************

#include "xbridgesessionlowlevel.h"
#include "util/util.h"
#include "util/logger.h"
#include "dht/dht.h"

#include <boost/asio.hpp>
#include <boost/asio/buffer.hpp>

//*****************************************************************************
//*****************************************************************************
XBridgeSessionLowLevel::XBridgeSessionLowLevel()
{

}

//*****************************************************************************
//*****************************************************************************
void XBridgeSessionLowLevel::start(XBridge::SocketPtr socket)
{
    // DEBUG_TRACE();

    LOG() << "client connected " << socket.get();

    m_socket = socket;

    doReadHeader(XBridgePacketPtr(new XBridgePacket));
}

//*****************************************************************************
//*****************************************************************************
void XBridgeSessionLowLevel::disconnect()
{
    // DEBUG_TRACE();

    m_socket->close();

    LOG() << "client disconnected " << m_socket.get();

    XBridgeApp * app = qobject_cast<XBridgeApp *>(qApp);
    app->storageClean(shared_from_this());
}

//*****************************************************************************
//*****************************************************************************
void XBridgeSessionLowLevel::doReadHeader(XBridgePacketPtr packet,
                                          const std::size_t offset)
{
    // DEBUG_TRACE();

    m_socket->async_read_some(
                boost::asio::buffer(packet->header()+offset,
                                    packet->headerSize-offset),
                boost::bind(&XBridgeSessionLowLevel::onReadHeader,
                            shared_from_this(),
                            packet, offset,
                            boost::asio::placeholders::error,
                            boost::asio::placeholders::bytes_transferred));
}

//*****************************************************************************
//*****************************************************************************
void XBridgeSessionLowLevel::onReadHeader(XBridgePacketPtr packet,
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
void XBridgeSessionLowLevel::doReadBody(XBridgePacketPtr packet,
                                        const std::size_t offset)
{
    // DEBUG_TRACE();

    m_socket->async_read_some(
                boost::asio::buffer(packet->data()+offset,
                                    packet->size()-offset),
                boost::bind(&XBridgeSessionLowLevel::onReadBody,
                            shared_from_this(),
                            packet, offset,
                            boost::asio::placeholders::error,
                            boost::asio::placeholders::bytes_transferred));
}

//*****************************************************************************
//*****************************************************************************
void XBridgeSessionLowLevel::onReadBody(XBridgePacketPtr packet,
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
bool XBridgeSessionLowLevel::processPacket(XBridgePacketPtr packet)
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
bool XBridgeSessionLowLevel::encryptPacket(XBridgePacketPtr /*packet*/)
{
    // DEBUG_TRACE();
    // TODO implement this
    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSessionLowLevel::decryptPacket(XBridgePacketPtr /*packet*/)
{
    // DEBUG_TRACE();
    // TODO implement this
    return true;
}
