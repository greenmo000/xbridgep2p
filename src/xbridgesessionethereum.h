//*****************************************************************************
//*****************************************************************************

#ifndef XBRIFGESSESSIONETHERIUM_H
#define XBRIFGESSESSIONETHERIUM_H

#include "xbridge.h"
#include "xbridgepacket.h"
#include "xbridgesession.h"
#include "xbridgetransaction.h"
#include "FastDelegate.h"
#include "util/uint256.h"

#include <memory>
#include <set>
#include <boost/thread/mutex.hpp>
#include <boost/noncopyable.hpp>

//*****************************************************************************
//*****************************************************************************
class XBridgeSessionEthereum
        : public XBridgeSession
{
public:
    XBridgeSessionEthereum();
    XBridgeSessionEthereum(const WalletParam & wallet);

    virtual ~XBridgeSessionEthereum();

public:
    std::shared_ptr<XBridgeSessionEthereum> shared_from_this()
    {
        return std::static_pointer_cast<XBridgeSessionEthereum>(XBridgeSession::shared_from_this());
    }

private:
    virtual void init();

protected:
    virtual void requestAddressBook();

    virtual bool revertXBridgeTransaction(const uint256 & id);

    virtual bool processTransactionCreate(XBridgePacketPtr packet);
    virtual bool processTransactionSign(XBridgePacketPtr packet);
    virtual bool processTransactionCommit(XBridgePacketPtr packet);

private:
    XBridge::SocketPtr m_socket;

    typedef std::map<const int, fastdelegate::FastDelegate1<XBridgePacketPtr, bool> > PacketProcessorsMap;
    PacketProcessorsMap m_processors;

    const uint64_t m_fee = 420000000000000;
};

typedef std::shared_ptr<XBridgeSessionEthereum> XBridgeSessionEtheriumPtr;

#endif // XBRIFGESSESSIONETHERIUM_H
