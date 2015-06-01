//*****************************************************************************
//*****************************************************************************

#ifndef XBRIDGESESSION_H
#define XBRIDGESESSION_H

#include "xbridge.h"
#include "xbridgepacket.h"
#include "FastDelegate.h"

#include <memory>
#include <boost/noncopyable.hpp>

//*****************************************************************************
//*****************************************************************************
class XBridgeSession
        : public std::enable_shared_from_this<XBridgeSession>
        , private boost::noncopyable
{
public:
    XBridgeSession();

    void start(XBridge::SocketPtr socket);

    bool sendXBridgeMessage(const std::vector<unsigned char> & message);

    bool processPacket(XBridgePacketPtr packet);

    void checkFinishedTransactions();

private:
    void disconnect();

    void doReadHeader(XBridgePacketPtr packet,
                      const std::size_t offset = 0);
    void onReadHeader(XBridgePacketPtr packet,
                      const std::size_t offset,
                      const boost::system::error_code & error,
                      std::size_t transferred);

    void doReadBody(XBridgePacketPtr packet,
                    const std::size_t offset = 0);
    void onReadBody(XBridgePacketPtr packet,
                    const std::size_t offset,
                    const boost::system::error_code & error,
                    std::size_t transferred);

    const unsigned char * myaddr() const;

    bool encryptPacket(XBridgePacketPtr packet);
    bool decryptPacket(XBridgePacketPtr packet);

    void sendPacket(const std::vector<unsigned char> & to, XBridgePacketPtr packet);

    // return true if packet not for me, relayed
    bool relayPacket(XBridgePacketPtr packet);

private:
    bool processInvalid(XBridgePacketPtr packet);
    bool processAnnounceAddresses(XBridgePacketPtr packet);
    bool processXBridgeMessage(XBridgePacketPtr packet);
    bool processXBridgeBroadcastMessage(XBridgePacketPtr packet);

    bool processTransaction(XBridgePacketPtr packet);
    bool processTransactionHoldApply(XBridgePacketPtr packet);
    bool processTransactionInitialized(XBridgePacketPtr packet);
    bool processTransactionCreated(XBridgePacketPtr packet);
    bool processTransactionSigned(XBridgePacketPtr packet);
    bool processTransactionCommited(XBridgePacketPtr packet);
    // bool processTransactionConfirmed(XBridgePacketPtr packet);
    bool processTransactionCancel(XBridgePacketPtr packet);

    bool processBitcoinTransactionHash(XBridgePacketPtr packet);

private:
    XBridge::SocketPtr m_socket;

    typedef std::map<const int, fastdelegate::FastDelegate1<XBridgePacketPtr, bool> > PacketProcessorsMap;
    PacketProcessorsMap m_processors;
};

typedef std::shared_ptr<XBridgeSession> XBridgeSessionPtr;

#endif // XBRIDGESESSION_H
