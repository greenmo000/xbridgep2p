//*****************************************************************************
//*****************************************************************************

#ifndef XBRIDGESESSION_H
#define XBRIDGESESSION_H

#include "xbridge.h"
#include "xbridgesessionlowlevel.h"
#include "xbridgepacket.h"
#include "FastDelegate.h"

#include <memory>
#include <boost/noncopyable.hpp>

//*****************************************************************************
//*****************************************************************************
class XBridgeSession
        : public XBridgeSessionLowLevel
        , public std::enable_shared_from_this<XBridgeSession>
        , private boost::noncopyable
{
public:
    XBridgeSession();

    bool sendXBridgeMessage(const std::vector<unsigned char> & message);

    bool encryptPacket(XBridgePacketPtr packet);
    bool decryptPacket(XBridgePacketPtr packet);

private:
    bool processInvalid(XBridgePacketPtr packet);
    bool processAnnounceAddresses(XBridgePacketPtr packet);
    bool processXBridgeMessage(XBridgePacketPtr packet);
    bool processXBridgeBroadcastMessage(XBridgePacketPtr packet);

    bool processTransaction(XBridgePacketPtr packet);
    bool processTransactionHoldApply(XBridgePacketPtr packet);
    // bool processTransactionPayApply(XBridgePacketPtr packet);
    // bool processTransactionCommitApply(XBridgePacketPtr packet);
    bool processTransactionCancel(XBridgePacketPtr packet);

    bool processBitcoinTransactionHash(XBridgePacketPtr packet);
};

typedef std::shared_ptr<XBridgeSession> XBridgeSessionPtr;

#endif // XBRIDGESESSION_H
