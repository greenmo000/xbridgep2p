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
    XBridgeTransactionMember() {}

    bool isEmpty() const { return m_sourceAddr.empty() || m_destAddr.empty(); }

    const std::vector<unsigned char> & source() const       { return m_sourceAddr; }
    void setSource(const std::vector<unsigned char> & addr) { m_sourceAddr = addr; }
    const std::vector<unsigned char> & dest() const         { return m_destAddr; }
    void setDest(const std::vector<unsigned char> & addr)   { m_destAddr = addr; }

private:
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
    XBridgeTransaction(const std::vector<unsigned char> & sourceAddr,
                       const std::string & sourceCurrency,
                       const boost::uint32_t sourceAmount,
                       const std::vector<unsigned char> & destAddr,
                       const std::string & destCurrency,
                       const boost::uint32_t destAmount);
    ~XBridgeTransaction();

    // state of transaction
    State state() const;

    bool isValid() const;
    bool isExpired() const;

    // hash of transaction
    uint256 hash1() const;
    uint256 hash2() const;

    bool tryJoin(const XBridgeTransactionPtr other);

private:
    State                      m_state;

    std::string                m_sourceCurrency;
    std::string                m_destCurrency;

    XBridgeTransactionMember   m_first;
    XBridgeTransactionMember   m_second;

    boost::uint32_t            m_sourceAmount;
    boost::uint32_t            m_destAmount;
};

#endif // XBRIDGETRANSACTION_H
