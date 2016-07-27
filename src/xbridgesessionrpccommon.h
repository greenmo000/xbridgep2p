//*****************************************************************************
//*****************************************************************************

#ifndef XBRIDGESESSIONRPCCOMMON_H
#define XBRIDGESESSIONRPCCOMMON_H

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
class XBridgeSessionRpc
        : public XBridgeSession
{
public:
    XBridgeSessionRpc();
    XBridgeSessionRpc(const WalletParam & wallet);
    virtual ~XBridgeSessionRpc();

public:
    std::shared_ptr<XBridgeSessionRpc> shared_from_this()
    {
        return std::static_pointer_cast<XBridgeSessionRpc>(XBridgeSession::shared_from_this());
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

typedef std::shared_ptr<XBridgeSessionRpc> XBridgeSessionRpcPtr;

#endif // XBRIDGESESSIONRPCCOMMON_H
