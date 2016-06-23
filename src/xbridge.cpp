//*****************************************************************************
//*****************************************************************************

#include "xbridge.h"
#include "xbridgesession.h"
#include "xbridgesessionetherium.h"
#include "xbridgeapp.h"
#include "util/logger.h"
#include "util/settings.h"

#include <boost/date_time/posix_time/posix_time.hpp>

//*****************************************************************************
//*****************************************************************************
XBridge::XBridge()
    : m_timerIoWork(new boost::asio::io_service::work(m_timerIo))
    , m_timerThread(boost::bind(&boost::asio::io_service::run, &m_timerIo))
    , m_timer(m_timerIo, boost::posix_time::seconds(TIMER_INTERVAL))
{
    try
    {
        // services and threas
        for (int i = 0; i < THREAD_COUNT; ++i)
        {
            IoServicePtr ios(new boost::asio::io_service);

            m_services.push_back(ios);
            m_works.push_back(WorkPtr(new boost::asio::io_service::work(*ios)));

            m_threads.create_thread(boost::bind(&boost::asio::io_service::run, ios));
        }

        m_timer.async_wait(boost::bind(&XBridge::onTimer, this));

        // sessions
        XBridgeApp & app = XBridgeApp::instance();
        {
            Settings & s = settings();
            std::vector<std::string> wallets = s.exchangeWallets();
            for (std::vector<std::string>::iterator i = wallets.begin(); i != wallets.end(); ++i)
            {
                std::string label         = s.get<std::string>(*i + ".Title");
                std::string address       = s.get<std::string>(*i + ".Address");
                std::string ip            = s.get<std::string>(*i + ".Ip");
                std::string port          = s.get<std::string>(*i + ".Port");
                std::string user          = s.get<std::string>(*i + ".Username");
                std::string passwd        = s.get<std::string>(*i + ".Password");
                std::string prefix        = s.get<std::string>(*i + ".AddressPrefix");
                boost::uint64_t COIN      = s.get<boost::uint64_t>(*i + ".COIN", 0);
                boost::uint64_t minAmount = s.get<boost::uint64_t>(*i + ".MinimumAmount", 0);

                if (ip.empty() || port.empty() ||
                    user.empty() || passwd.empty() ||
                    prefix.empty() || COIN == 0)
                {
                    LOG() << "read wallet " << *i << " with empty parameters>";
                    continue;
                }
                else
                {
                    LOG() << "read wallet " << *i << " [" << label << "] " << ip << ":" << port << " COIN=" << COIN;
                }

                XBridgeSessionPtr session;
                if (*i != "ETHER")
                {
                    session.reset(new XBridgeSession(*i, address,
                                                     ip, port,
                                                     user, passwd,
                                                     prefix, COIN, minAmount));
                }
                else
                {
                    session.reset(new XBridgeSessionEtherium(*i, address,
                                                             ip, port,
                                                             user, passwd,
                                                             prefix, COIN, minAmount));
                }
                app.addSession(session);
                // session->requestAddressBook();
            }
        }
    }
    catch (std::exception & e)
    {
        ERR() << e.what();
        ERR() << __FUNCTION__;
    }
}

//*****************************************************************************
//*****************************************************************************
void XBridge::run()
{
    m_threads.join_all();
}

//*****************************************************************************
//*****************************************************************************
void XBridge::stop()
{
    m_timer.cancel();
    m_timerIo.stop();
    m_timerIoWork.reset();

    for (IoServicePtr & i : m_services)
    {
        i->stop();
    }
    for (WorkPtr & i : m_works)
    {
        i.reset();
    }

    m_threads.join_all();
}

//******************************************************************************
//******************************************************************************
void XBridge::onTimer()
{
    // DEBUG_TRACE();

    {
        m_services.push_back(m_services.front());
        m_services.pop_front();

        // XBridgeSessionPtr session(new XBridgeSession);
        XBridgeSessionPtr session = XBridgeApp::instance().serviceSession();

        IoServicePtr io = m_services.front();

        // call check expired transactions
        io->post(boost::bind(&XBridgeSession::checkFinishedTransactions, session));

        // send list of wallets (broadcast)
        // io->post(boost::bind(&XBridgeSession::sendListOfWallets, session));

        // send transactions list
        io->post(boost::bind(&XBridgeSession::sendListOfTransactions, session));

        // erase expired tx
        io->post(boost::bind(&XBridgeSession::eraseExpiredPendingTransactions, session));

        // check unconfirmed tx
        io->post(boost::bind(&XBridgeSession::checkUnconfirmedTx, session));

        // resend addressbook
        // io->post(boost::bind(&XBridgeSession::resendAddressBook, session));
        io->post(boost::bind(&XBridgeSession::getAddressBook, session));
    }

    m_timer.expires_at(m_timer.expires_at() + boost::posix_time::seconds(TIMER_INTERVAL));
    m_timer.async_wait(boost::bind(&XBridge::onTimer, this));
}
