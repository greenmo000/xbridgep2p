//*****************************************************************************
//*****************************************************************************

#include "xbridgetransaction.h"
#include "util/logger.h"
#include "util/util.h"


//*****************************************************************************
//*****************************************************************************
XBridgeTransaction::XBridgeTransaction(const std::vector<unsigned char> & sourceAddr,
                                       const boost::uint32_t sourceAmount,
                                       const std::vector<unsigned char> & destAddr,
                                       const boost::uint32_t destAmount)
    : m_state(trNew)
    , m_sourceAmount(sourceAmount)
    , m_destAmount(destAmount)
{
    m_first.setSource(sourceAddr);
    m_first.setDest(destAddr);
}

//*****************************************************************************
//*****************************************************************************
XBridgeTransaction::~XBridgeTransaction()
{
}

//*****************************************************************************
// state of transaction
//*****************************************************************************
XBridgeTransaction::State XBridgeTransaction::state() const
{
    return m_state;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeTransaction::isValid() const
{
    // TODO
    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeTransaction::isExpired() const
{
    // TODO
    return false;
}


//*****************************************************************************
//*****************************************************************************
uint256 XBridgeTransaction::hash1() const
{
    return util::hash(m_sourceCurrency.begin(), m_sourceCurrency.end(),
                      BEGIN(m_sourceAmount), END(m_sourceAmount),
                      m_destCurrency.begin(), m_destCurrency.end(),
                      BEGIN(m_destAmount), END(m_destAmount));
}

//*****************************************************************************
//*****************************************************************************
uint256 XBridgeTransaction::hash2() const
{
    return util::hash(m_destCurrency.begin(), m_destCurrency.end(),
                      BEGIN(m_destAmount), END(m_destAmount),
                      m_sourceCurrency.begin(), m_sourceCurrency.end(),
                      BEGIN(m_sourceAmount), END(m_sourceAmount));
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeTransaction::tryJoin(const XBridgeTransactionPtr other)
{
    DEBUG_TRACE();

    if (m_state != trNew || other->state() != trNew)
    {
        // can be joined only new transactions
        return false;
    }

    if (m_sourceCurrency != other->m_destCurrency ||
        m_destCurrency != other->m_sourceCurrency)
    {
        // not same currencies
        ERR() << "not same currencies. transaction not joined" << __FUNCTION__;
        // assert(false || "not same currencies. transaction not joined");
        return false;
    }

    if (m_sourceAmount != other->m_destAmount ||
        m_destAmount != other->m_sourceAmount)
    {
        // not same currencies
        ERR() << "not same amount. transaction not joined" << __FUNCTION__;
        // assert(false || "not same amount. transaction not joined");
        return false;
    }

    // join second member
    m_second.setDest(other->m_first.source());
    m_second.setSource(other->m_first.dest());

    m_state = trJoined;

    return true;
}
