//*****************************************************************************
//*****************************************************************************

#ifndef XBRIDGETRANSACTION_H
#define XBRIDGETRANSACTION_H

#include "util/uint256.h"
#include "xbridgetransactionmember.h"
#include "key.h"

#include <vector>
#include <string>

#include <boost/cstdint.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/date_time/posix_time/ptime.hpp>

//*****************************************************************************
//*****************************************************************************
class XBridgeTransaction;
typedef boost::shared_ptr<XBridgeTransaction> XBridgeTransactionPtr;

//*****************************************************************************
//*****************************************************************************
class XBridgeTransaction
{
public:
    // see strState when editing
    enum State
    {
        trInvalid = 0,
        trNew,
        trJoined,
        trHold,
        trInitialized,
        trCreated,
        trSigned,
        trCommited,
        trConfirmed,
        trFinished,
        trCancelled,
        trDropped
    };

    enum
    {
        // transaction lock time base, in seconds, 10 min
        lockTime = 600,

        // pending transaction ttl in seconds, 72 hours
        pendingTTL = 259200,

        // transaction ttl in seconds, 10 min
        TTL = 600
    };

public:
    XBridgeTransaction();
    XBridgeTransaction(const uint256 & id,
                       const std::string & sourceAddr,
                       const std::string & sourceCurrency,
                       const uint64_t & sourceAmount,
                       const std::string & destAddr,
                       const std::string & destCurrency,
                       const uint64_t & destAmount,
                       const uint32_t & tax,
                       const std::vector<unsigned char> & taxAddress);
    ~XBridgeTransaction();

    uint256 id() const;

    // state of transaction
    State state() const;
    // update state counter and update state
    State increaseStateCounter(State state, const std::string & from);

    static std::string strState(const State state);
    std::string strState() const;

    void updateTimestamp();

    bool isFinished() const;
    bool isValid() const;
    bool isExpired() const;

    void cancel();
    void drop();
    void finish();

    bool confirm(const uint256 & hash);

    // hash of transaction
    uint256 hash1() const;
    uint256 hash2() const;

    // uint256                    firstId() const;
    std::string                a_address() const;
    std::string                a_destination() const;
    std::string                a_currency() const;
    boost::uint64_t            a_amount() const;
    std::string                a_rawPayTx() const;
    std::string                a_rawRefTx() const;
    uint256                    a_txHash() const;

    // uint160                    a_x() const;
    CPubKey                    a_pk1() const;

    // uint256                    secondId() const;
    std::string                b_address() const;
    std::string                b_destination() const;
    std::string                b_currency() const;
    boost::uint64_t            b_amount() const;
    std::string                b_rawPayTx() const;
    std::string                b_rawRefTx() const;
    uint256                    b_txHash() const;

    uint160                    b_x() const;
    CPubKey                    b_pk1() const;

    std::string                fromXAddr(const std::vector<unsigned char> & xaddr) const;

    boost::uint32_t            tax() const;
    std::vector<unsigned char> a_taxAddress() const;
    std::vector<unsigned char> b_taxAddress() const;

    bool tryJoin(const XBridgeTransactionPtr other);

    // std::vector<unsigned char> opponentAddress(const std::vector<unsigned char> & addr);

    bool                       setKeys(const std::string & addr,
                                       const uint160 & x,
                                       const CPubKey & pk);
    bool                       setRawTx(const std::string & addr,
                                        const std::string & payTx,
                                        const std::string & refTx);
    bool                       setTxHash(const std::string & addr,
                                         const uint256 & hash);

public:
    boost::mutex               m_lock;

private:
    uint256                    m_id;

    boost::posix_time::ptime   m_created;

    State                      m_state;
    // unsigned int               m_stateCounter;
    bool                       m_a_stateChanged;
    bool                       m_b_stateChanged;

    unsigned int               m_confirmationCounter;

    std::string                m_sourceCurrency;
    std::string                m_destCurrency;

    boost::uint64_t            m_sourceAmount;
    boost::uint64_t            m_destAmount;

    std::string                m_rawpaytx1;
    std::string                m_rawrevtx1;
    std::string                m_rawpaytx2;
    std::string                m_rawrevtx2;

    uint256                    m_txhash1;
    uint256                    m_txhash2;

    XBridgeTransactionMember   m_a;
    XBridgeTransactionMember   m_b;

    uint160                    m_a_x;
    uint160                    m_b_x;

    CPubKey                    m_a_pk1;
    CPubKey                    m_b_pk1;

    boost::uint32_t            m_tax;
    std::vector<unsigned char> m_a_taxAddress;
    std::vector<unsigned char> m_b_taxAddress;
};

#endif // XBRIDGETRANSACTION_H
