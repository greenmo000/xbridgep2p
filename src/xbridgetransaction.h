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
    XBridgeTransaction(const uint256     & id,
                       const std::string & sourceAddr,
                       const std::string & sourceCurrency,
                       const uint64_t    & sourceAmount,
                       const std::string & destAddr,
                       const std::string & destCurrency,
                       const uint64_t    & destAmount,
                       const uint32_t    & tax,
                       const std::string & taxAddress);
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

    bool confirm(const std::string & id);

    // hash of transaction
    uint256 hash1() const;
    uint256 hash2() const;

    // uint256                    firstId() const;
    std::string                a_address() const;
    std::string                a_destination() const;
    std::string                a_currency() const;
    boost::uint64_t            a_amount() const;
    std::string                a_prevtxs() const;
    std::string                a_payTx() const;
    std::string                a_refTx() const;
    std::string                a_bintxid() const;

    // CPubKey                    a_x() const;
    CPubKey                    a_pk1() const;

    // uint256                    secondId() const;
    std::string                b_address() const;
    std::string                b_destination() const;
    std::string                b_currency() const;
    boost::uint64_t            b_amount() const;
    std::string                b_prevtxs() const;
    std::string                b_payTx() const;
    std::string                b_refTx() const;
    std::string                b_bintxid() const;

    CPubKey                    b_x() const;
    CPubKey                    b_pk1() const;

    std::string                fromXAddr(const std::vector<unsigned char> & xaddr) const;

    boost::uint32_t            tax() const;
    std::string                a_taxAddress() const;
    std::string                b_taxAddress() const;

    bool tryJoin(const XBridgeTransactionPtr other);

    // std::vector<unsigned char> opponentAddress(const std::vector<unsigned char> & addr);

    bool                       setKeys(const std::string & addr,
                                       const CPubKey & x,
                                       const CPubKey & pk);
    bool                       setRefTx(const std::string & addr,
                                        const std::string & prevtxs,
                                        const std::string & refTx);
    bool                       setBinTxId(const std::string & addr,
                                          const std::string & id);

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

    std::string                m_prevtxs1;
    std::string                m_prevtxs2;

    std::string                m_rawpaytx1;
    std::string                m_rawrevtx1;
    std::string                m_rawpaytx2;
    std::string                m_rawrevtx2;

    std::string                m_bintxid1;
    std::string                m_bintxid2;

    XBridgeTransactionMember   m_a;
    XBridgeTransactionMember   m_b;

    CPubKey                    m_a_x;
    CPubKey                    m_b_x;

    CPubKey                    m_a_pk1;
    CPubKey                    m_b_pk1;

    boost::uint32_t            m_tax;
    std::string                m_a_taxAddress;
    std::string                m_b_taxAddress;
};

#endif // XBRIDGETRANSACTION_H
