//*****************************************************************************
//*****************************************************************************

#ifndef XBRIDGEAPP_H
#define XBRIDGEAPP_H

#include "xbridge.h"
#include "xbridgesession.h"
#include "util/uint256.h"
#include "xbridgetransactiondescr.h"

#include <thread>
#include <atomic>
#include <vector>
#include <map>
#include <tuple>
#include <set>
#include <queue>

#ifdef WIN32
// #include <Ws2tcpip.h>
#endif

#ifndef NO_GUI
#include <QApplication>
#endif

namespace rpc
{
class AcceptedConnection;
}

//*****************************************************************************
//*****************************************************************************
class XBridgeApp
{
    typedef std::vector<unsigned char> UcharVector;

    friend void callback(void * closure, int event,
                         const unsigned char * info_hash,
                         const void * data, size_t data_len);

private:
    XBridgeApp();
    virtual ~XBridgeApp();

public:
    static XBridgeApp & instance();

    static std::string version();

    bool init(int argc, char *argv[]);

    int exec();


    uint256 sendXBridgeTransaction(const std::string & from,
                                   const std::string & fromCurrency,
                                   const uint64_t & fromAmount,
                                   const std::string & to,
                                   const std::string & toCurrency,
                                   const uint64_t & toAmount);
    bool sendPendingTransaction(XBridgeTransactionDescrPtr & ptr);

    uint256 acceptXBridgeTransaction(const uint256 & id,
                                     const std::string & from,
                                     const std::string & to);
    bool sendAcceptingTransaction(XBridgeTransactionDescrPtr & ptr);

    bool cancelXBridgeTransaction(const uint256 & id, const TxCancelReason & reason);
    bool sendCancelTransaction(const uint256 & txid, const TxCancelReason & reason);

    int peersCount() const;

//signals:
//    void showLogMessage(const QString & msg);

public:
    // const unsigned char * myid() const { return m_myid; }

    bool initDht();
    bool initRpc();

    bool stop();

    bool signalRpcStopActive() const;

    // void logMessage(const QString & msg);

    XBridgeSessionPtr sessionByCurrency(const std::string & currency) const;

    // store session
    void addSession(XBridgeSessionPtr session);
    // store session addresses in local table
    void storageStore(XBridgeSessionPtr session, const unsigned char * data);
    // clear local table
    void storageClean(XBridgeSessionPtr session);

    bool isLocalAddress(const std::vector<unsigned char> & id);
    bool isKnownMessage(const std::vector<unsigned char> & message);
    void addToKnown(const std::vector<unsigned char> & message);

    void handleRpcRequest(rpc::AcceptedConnection * conn);

public:// slots:
    // generate new id
    void onGenerate();
    // dump local table
    void onDump();
    // search id
    void onSearch(const std::string & id);
    // send messave via xbridge
    void onSend(const UcharVector & from, const UcharVector & message);
    void onSend(const UcharVector & from, const XBridgePacketPtr packet);
    void onSend(const UcharVector & from, const UcharVector & id, const std::vector<unsigned char> & message);
    void onSend(const UcharVector & from, const UcharVector & id, const XBridgePacketPtr packet);
    // call when message from xbridge network received
    void onMessageReceived(const std::vector<unsigned char> & id, const std::vector<unsigned char> & message);
    // broadcast message
    void onBroadcastReceived(const std::vector<unsigned char> & message);

    void storeAddressBookEntry(const std::string & currency,
                               const std::string & name,
                               const std::string & address);
    void resendAddressBook();

    void getAddressBook();

    void checkUnconfirmedTx();

    XBridgeSessionPtr serviceSession();

public:
    static void sleep(const unsigned int umilliseconds);

private:
    void dhtThreadProc();
    void bridgeThreadProc();
    void rpcThreadProc();
    void rpcHandlerProc(rpc::AcceptedConnection * conn);

private:
    unsigned char     m_myid[20];

    boost::thread_group m_threads;
    // std::thread       m_dhtThread;
    std::atomic<bool> m_dhtStarted;
    std::atomic<bool> m_dhtStop;
    std::atomic<bool> m_rpcStop;

    std::atomic<bool> m_signalGenerate;
    std::atomic<bool> m_signalDump;
    std::atomic<bool> m_signalSearch;
    std::atomic<bool> m_signalSend;

    // from, to, packet, resend_flag
    typedef std::tuple<UcharVector, UcharVector, UcharVector, bool> MessagePair;

    std::list<std::string> m_searchStrings;
    std::list<MessagePair> m_messages;

    const bool        m_ipv4;
    const bool        m_ipv6;

    sockaddr_in       m_sin;
    sockaddr_in6      m_sin6;
    unsigned short    m_dhtPort;

    std::vector<sockaddr_storage> m_nodes;

    // unsigned short    m_bridgePort;
    XBridgePtr        m_bridge;

    mutable boost::mutex m_sessionsLock;
    typedef std::map<std::vector<unsigned char>, XBridgeSessionPtr> SessionAddrMap;
    SessionAddrMap m_sessionAddrs;
    typedef std::map<std::string, XBridgeSessionPtr> SessionIdMap;
    SessionIdMap m_sessionIds;
    typedef std::queue<XBridgeSessionPtr> SessionQueue;
    SessionQueue m_sessionQueue;

    // service session
    XBridgeSessionPtr m_serviceSession;


    boost::mutex m_messagesLock;
    typedef std::set<uint256> ProcessedMessages;
    ProcessedMessages m_processedMessages;

    boost::mutex m_addressBookLock;
    typedef std::tuple<std::string, std::string, std::string> AddressBookEntry;
    typedef std::vector<AddressBookEntry> AddressBook;
    AddressBook m_addressBook;
    std::set<std::string> m_addresses;

public:
    static boost::mutex                                  m_txLocker;
    static std::map<uint256, XBridgeTransactionDescrPtr> m_pendingTransactions;
    static std::map<uint256, XBridgeTransactionDescrPtr> m_transactions;

    static boost::mutex                                  m_txHelperLocker;
    static std::map<uint256, XBridgeTransactionDescrPtr> m_helperTransactions;

    static boost::mutex                                  m_txUnconfirmedLocker;
    static std::map<uint256, XBridgeTransactionDescrPtr> m_unconfirmed;

#ifndef NO_GUI
    std::shared_ptr<QCoreApplication> m_app;
#endif
};

#endif // XBRIDGEAPP_H
