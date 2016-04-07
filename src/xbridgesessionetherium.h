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
        , private boost::noncopyable
{
public:
    XBridgeSessionEtherium();
    XBridgeSessionEtherium(const std::string & currency,
                   const std::string & address,
                   const std::string & port,
                   const std::string & user,
                   const std::string & passwd,
                   const std::string & prefix,
                   const boost::uint64_t & COIN);

    virtual void sendListOfWallets();
    virtual void sendListOfTransactions();
    virtual void checkFinishedTransactions();
    virtual void eraseExpiredPendingTransactions();

    virtual void resendAddressBook();
    virtual void sendAddressbookEntry(const std::string & currency,
                              const std::string & name,
                              const std::string & address);

    virtual void getAddressBook();
    virtual void requestAddressBook();

private:
    virtual void init();

    const unsigned char * myaddr() const;

private:
    virtual bool processInvalid(XBridgePacketPtr packet);
    virtual bool processZero(XBridgePacketPtr packet);
    virtual bool processAnnounceAddresses(XBridgePacketPtr packet);
    virtual bool processXChatMessage(XBridgePacketPtr packet);

    virtual bool processTransaction(XBridgePacketPtr packet);
    virtual bool processTransactionHoldApply(XBridgePacketPtr packet);
    virtual bool processTransactionInitialized(XBridgePacketPtr packet);
    virtual bool processTransactionCreated(XBridgePacketPtr packet);
    virtual bool processTransactionSigned(XBridgePacketPtr packet);
    virtual bool processTransactionCommited(XBridgePacketPtr packet);
    virtual bool processTransactionConfirm(XBridgePacketPtr packet);
    virtual bool processTransactionConfirmed(XBridgePacketPtr packet);
    virtual bool processTransactionCancel(XBridgePacketPtr packet);

    virtual bool finishTransaction(XBridgeTransactionPtr tr);
    virtual bool sendCancelTransaction(const uint256 & txid);
    virtual bool rollbackTransaction(XBridgeTransactionPtr tr);
    virtual bool revertXBridgeTransaction(const uint256 & id);

    virtual bool processBitcoinTransactionHash(XBridgePacketPtr packet);

    virtual bool processAddressBookEntry(XBridgePacketPtr packet);

    virtual bool processPendingTransaction(XBridgePacketPtr packet);
    virtual bool processTransactionHold(XBridgePacketPtr packet);
    virtual bool processTransactionInit(XBridgePacketPtr packet);
    virtual bool processTransactionCreate(XBridgePacketPtr packet);
    virtual bool processTransactionCreateBTC(XBridgePacketPtr packet);
    virtual bool processTransactionSign(XBridgePacketPtr packet);
    virtual bool processTransactionCommit(XBridgePacketPtr packet);
    virtual bool processTransactionFinished(XBridgePacketPtr packet);
    virtual bool processTransactionRollback(XBridgePacketPtr packet);
    virtual bool processTransactionDropped(XBridgePacketPtr packet);

private:
    XBridge::SocketPtr m_socket;

    typedef std::map<const int, fastdelegate::FastDelegate1<XBridgePacketPtr, bool> > PacketProcessorsMap;
    PacketProcessorsMap m_processors;

    std::string       m_currency;
    std::string       m_address;
    std::string       m_port;
    std::string       m_user;
    std::string       m_passwd;
    std::string       m_prefix;
    boost::uint64_t   m_COIN;
};

typedef std::shared_ptr<XBridgeSessionEtherium> XBridgeSessionEtheriumPtr;

#endif // XBRIFGESSESSIONETHERIUM_H
