//*****************************************************************************
//*****************************************************************************

#include "xbridgeexchange.h"
#include "util/logger.h"
#include "util/settings.h"

#include <algorithm>

//*****************************************************************************
//*****************************************************************************
XBridgeExchange::XBridgeExchange()
{
}

//*****************************************************************************
//*****************************************************************************
XBridgeExchange::~XBridgeExchange()
{
}

//*****************************************************************************
//*****************************************************************************
// static
XBridgeExchange & XBridgeExchange::instance()
{
    static XBridgeExchange e;
    return e;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeExchange::init()
{
    Settings & s = Settings::instance();

    std::vector<std::string> wallets = s.exchangeWallets();
    for (std::vector<std::string>::iterator i = wallets.begin(); i != wallets.end(); ++i)
    {
        std::string label   = s.get<std::string>(*i + ".Title");
        std::string address = s.get<std::string>(*i + ".Address");

        if (address.empty())
        {
            LOG() << "read wallet " << *i << " with empty address>";
            continue;
        }

        m_wallets[*i].title   = label;
        m_wallets[*i].address = address;

        LOG() << "read wallet " << *i << " \"" << label << "\" address <" << address << ">";
    }

    if (isEnabled())
    {
        LOG() << "exchange enabled";
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeExchange::isEnabled()
{
    return m_wallets.size() > 0;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeExchange::haveConnectedWallet(const std::string & walletName)
{
    return m_wallets.count(walletName);
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeExchange::createTransaction(const std::vector<unsigned char> & sourceAddr,
                                        const std::string & sourceCurrency,
                                        const boost::uint32_t sourceAmount,
                                        const std::vector<unsigned char> & destAddr,
                                        const std::string & destCurrency,
                                        const boost::uint32_t destAmount,
                                        uint256 & transactionId)
{
    DEBUG_TRACE();

    transactionId = uint256();

    XBridgeTransactionPtr tr(new XBridgeTransaction(sourceAddr, sourceCurrency,
                                                    sourceAmount,
                                                    destAddr, destCurrency,
                                                    destAmount));
    if (!tr->isValid())
    {
        return false;
    }

    uint256 h = tr->hash2();

    {
        boost::mutex::scoped_lock l(m_transactionsLock);

        if (!m_transactions.count(h))
        {
            // new transaction
            h = tr->hash1();
            m_transactions[h] = tr;
        }
        else
        {
            if (m_transactions[h]->isExpired())
            {
                // if expired - delete old transaction
                m_transactions.erase(h);
                h = tr->hash1();
                m_transactions[h] = tr;
            }
            else
            {
                // try join with existing transaction
                if (!m_transactions[h]->tryJoin(tr))
                {
                    LOG() << "transaction not joined";
                    // return false;

                    // create new transaction
                    h = tr->hash1();
                    m_transactions[h] = tr;
                }
            }
        }
    }

    transactionId = h;

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeExchange::updateTransaction(const uint256 & hash)
{
    DEBUG_TRACE();

    m_walletTransactions.insert(hash);

    return true;
}

//*****************************************************************************
//*****************************************************************************
const XBridgeTransactionPtr XBridgeExchange::transaction(const uint256 & hash)
{
    boost::mutex::scoped_lock l(m_transactionsLock);

    if (!m_transactions.count(hash))
    {
        // return XBridgeTransaction::trInvalid;
        return XBridgeTransactionPtr(new XBridgeTransaction);
    }

    return m_transactions[hash];
}

//*****************************************************************************
//*****************************************************************************
std::vector<StringPair> XBridgeExchange::listOfWallets() const
{
    std::vector<StringPair> result;
    for (WalletList::const_iterator i = m_wallets.begin(); i != m_wallets.end(); ++i)
    {
        result.push_back(std::make_pair(i->first, i->second.title));
    }
    return result;
}
