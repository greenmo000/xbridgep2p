//*****************************************************************************
//*****************************************************************************

#ifndef XBRIDGETRANSACTION_H
#define XBRIDGETRANSACTION_H

#include "util/uint256.h"

#include <vector>
#include <string>

#include <boost/cstdint.hpp>
#include <boost/shared_ptr.hpp>

//*****************************************************************************
//*****************************************************************************
class XBridgeTransactionMember
{
public:
    XBridgeTransactionMember()                              {}
    XBridgeTransactionMember(const uint256 & id) : m_id(id) {}

    bool isEmpty() const { return m_sourceAddr.empty() || m_destAddr.empty(); }

    const uint256 id() const                                { return m_id; }
    const std::vector<unsigned char> & source() const       { return m_sourceAddr; }
    void setSource(const std::vector<unsigned char> & addr) { m_sourceAddr = addr; }
    const std::vector<unsigned char> & dest() const         { return m_destAddr; }
    void setDest(const std::vector<unsigned char> & addr)   { m_destAddr = addr; }

//    XBridgeTransactionMember(const XBridgeTransactionMember & other)
//        : m_id(other.m_id)
//        , m_sourceAddr(other.m_sourceAddr)
//        , m_destAddr(other.m_destAddr)
//        , m_transactionHash(other.m_transactionHash)
//    {
//    }

//    XBridgeTransactionMember & operator = (const XBridgeTransactionMember & other)
//    {
//        if (this != &other)
//        {
//            m_id              = other.m_id;
//            m_sourceAddr      = other.m_sourceAddr;
//            m_destAddr        = other.m_destAddr;
//            m_transactionHash = other.m_transactionHash;
//        }
//        return *this;
//    }

private:
    uint256                    m_id;
    std::vector<unsigned char> m_sourceAddr;
    std::vector<unsigned char> m_destAddr;
    uint256                    m_transactionHash;
};

//*****************************************************************************
//*****************************************************************************
class XBridgeTransaction;
typedef boost::shared_ptr<XBridgeTransaction> XBridgeTransactionPtr;

//*****************************************************************************
//*****************************************************************************
class XBridgeTransaction
{
public:
    enum State
    {
        trInvalid = 0,
        trNew,
        trJoined,
        trHold,
        trPaid,
        trFinished,
        trDropped
    };

public:
    XBridgeTransaction();
    XBridgeTransaction(const uint256 & id,
                       const std::vector<unsigned char> & sourceAddr,
                       const std::string & sourceCurrency,
                       const boost::uint64_t sourceAmount,
                       const std::vector<unsigned char> & destAddr,
                       const std::string & destCurrency,
                       const boost::uint64_t destAmount);
    ~XBridgeTransaction();

    uint256 id() const;
    // state of transaction
    State state() const;
    // update state counter and update state
    State increaseStateCounter(State state);

    bool isValid() const;
    bool isExpired() const;

    void drop();

    // hash of transaction
    uint256 hash1() const;
    uint256 hash2() const;

    const std::vector<unsigned char> firstAddress() const;
    const std::string firstCurrency() const;
    const std::vector<unsigned char> secondAddress() const;
    const std::string secondCurrency() const;

    bool tryJoin(const XBridgeTransactionPtr other);

private:
    uint256                    m_id;

    State                      m_state;
    unsigned int               m_stateCounter;

    std::string                m_sourceCurrency;
    std::string                m_destCurrency;

    boost::uint64_t            m_sourceAmount;
    boost::uint64_t            m_destAmount;

    XBridgeTransactionMember   m_first;
    XBridgeTransactionMember   m_second;
};

#endif // XBRIDGETRANSACTION_H
