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
class XBridgeSessionEtherium
        : public XBridgeSession
        , public std::enable_shared_from_this<XBridgeSessionEtherium>
{
public:
    using std::enable_shared_from_this<XBridgeSessionEtherium>::shared_from_this;

public:
    XBridgeSessionEtherium();
    XBridgeSessionEtherium(const std::string & currency,
                           const std::string & walletAddress,
                           const std::string & address,
                           const std::string & port,
                           const std::string & user,
                           const std::string & passwd,
                           const std::string & prefix,
                           const boost::uint64_t & COIN,
                           const boost::uint64_t & minAmount);

    virtual ~XBridgeSessionEtherium();

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

typedef std::shared_ptr<XBridgeSessionEtherium> XBridgeSessionEtheriumPtr;

#endif // XBRIFGESSESSIONETHERIUM_H
