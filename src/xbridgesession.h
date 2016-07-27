//*****************************************************************************
//*****************************************************************************

#ifndef XBRIDGESESSION_H
#define XBRIDGESESSION_H

#include "xbridge.h"
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
class XBridgeSession
        : public std::enable_shared_from_this<XBridgeSession>
        , private boost::noncopyable
{
public:
    XBridgeSession();
    XBridgeSession(const WalletParam & wallet);
    virtual ~XBridgeSession();

    const unsigned char * sessionAddr() const { return m_myid; }
    std::string currency() const  { return m_wallet.currency; }
    double      minAmount() const { return (double)m_wallet.minAmount / m_wallet.COIN; }

    void start(XBridge::SocketPtr socket);

    static bool checkXBridgePacketVersion(XBridgePacketPtr packet);

    bool sendXBridgeMessage(XBridgePacketPtr packet);
    bool takeXBridgeMessage(const std::vector<unsigned char> & message);

    bool processPacket(XBridgePacketPtr packet);

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

    void checkUnconfirmedTx();
    void requestUnconfirmedTx();

private:
    void init();

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

    // const unsigned char * myaddr() const;

    bool encryptPacket(XBridgePacketPtr packet);
    bool decryptPacket(XBridgePacketPtr packet);

protected:
    const unsigned char * myaddr() const;

    void sendPacket(const std::vector<unsigned char> & to, XBridgePacketPtr packet);
    void sendPacketBroadcast(XBridgePacketPtr packet);

    // return true if packet not for me, relayed
    bool relayPacket(XBridgePacketPtr packet);

    virtual std::string currencyToLog() const { return std::string("[") + m_wallet.currency + std::string("]"); }

protected:
    virtual bool processInvalid(XBridgePacketPtr packet);
    virtual bool processZero(XBridgePacketPtr packet);
    virtual bool processAnnounceAddresses(XBridgePacketPtr packet);
    virtual bool processXChatMessage(XBridgePacketPtr packet);

    virtual bool processTransaction(XBridgePacketPtr packet);
    virtual bool processTransactionAccepting(XBridgePacketPtr packet);
    virtual bool processTransactionHoldApply(XBridgePacketPtr packet);
    virtual bool processTransactionInitialized(XBridgePacketPtr packet);
    virtual bool processTransactionCreated(XBridgePacketPtr packet);
    virtual bool processTransactionSigned(XBridgePacketPtr packet);
    virtual bool processTransactionCommited(XBridgePacketPtr packet);
    virtual bool processTransactionConfirm(XBridgePacketPtr packet);
    virtual bool processTransactionConfirmed(XBridgePacketPtr packet);
    virtual bool processTransactionCancel(XBridgePacketPtr packet);

    virtual bool finishTransaction(XBridgeTransactionPtr tr);
    virtual bool sendCancelTransaction(const uint256 & txid,
                                       const TxCancelReason & reason);
    virtual bool rollbackTransaction(XBridgeTransactionPtr tr);
    virtual bool revertXBridgeTransaction(const uint256 & id);

    virtual bool processBitcoinTransactionHash(XBridgePacketPtr packet);

    virtual bool processAddressBookEntry(XBridgePacketPtr packet);

    virtual bool processPendingTransaction(XBridgePacketPtr packet);
    virtual bool processTransactionHold(XBridgePacketPtr packet);
    virtual bool processTransactionInit(XBridgePacketPtr packet);
    virtual bool processTransactionCreate(XBridgePacketPtr packet);
    virtual bool processTransactionSign(XBridgePacketPtr packet);
    virtual bool processTransactionCommit(XBridgePacketPtr packet);
    virtual bool processTransactionFinished(XBridgePacketPtr packet);
    virtual bool processTransactionRollback(XBridgePacketPtr packet);
    virtual bool processTransactionDropped(XBridgePacketPtr packet);

protected:
    unsigned char     m_myid[20];

    XBridge::SocketPtr m_socket;

    typedef std::map<const int, fastdelegate::FastDelegate1<XBridgePacketPtr, bool> > PacketProcessorsMap;
    PacketProcessorsMap m_processors;

protected:
    std::set<std::vector<unsigned char> > m_addressBook;

    WalletParam       m_wallet;

//    std::string       m_currency;
//    std::string       m_walletAddress;
//    std::string       m_address;
//    std::string       m_port;
//    std::string       m_user;
//    std::string       m_passwd;
//    std::string       m_prefix;
//    boost::uint64_t   m_COIN;
//    boost::uint64_t   m_minAmount;
};

typedef std::shared_ptr<XBridgeSession> XBridgeSessionPtr;

#endif // XBRIDGESESSION_H
