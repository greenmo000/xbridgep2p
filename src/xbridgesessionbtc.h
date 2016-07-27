//*****************************************************************************
//*****************************************************************************

#ifndef XBRIDGESESSIONBTC_H
#define XBRIDGESESSIONBTC_H

#include "xbridge.h"
#include "xbridgesession.h"
#include "xbridgepacket.h"
#include "xbridgetransaction.h"
#include "xbridgewallet.h"
#include "FastDelegate.h"
#include "util/uint256.h"

#include <memory>
#include <set>
#include <boost/thread/mutex.hpp>
#include <boost/noncopyable.hpp>

//*****************************************************************************
//*****************************************************************************
class XBridgeSessionBtc
        : public XBridgeSession
{
public:
    XBridgeSessionBtc();
    XBridgeSessionBtc(const WalletParam & wallet);
    virtual ~XBridgeSessionBtc();

public:
    std::shared_ptr<XBridgeSessionBtc> shared_from_this()
    {
        return std::static_pointer_cast<XBridgeSessionBtc>(XBridgeSession::shared_from_this());
    }

private:
    void init();

protected:
    virtual bool processTransactionCreate(XBridgePacketPtr packet);

protected:
    XBridge::SocketPtr m_socket;

    typedef std::map<const int, fastdelegate::FastDelegate1<XBridgePacketPtr, bool> > PacketProcessorsMap;
    PacketProcessorsMap m_processors;
};

typedef std::shared_ptr<XBridgeSessionBtc> XBridgeSessionBtcPtr;

#endif // XBRIDGESESSIONBTC_H
