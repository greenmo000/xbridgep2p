//*****************************************************************************
//*****************************************************************************

#include "xbridgesession.h"
#include "xbridgeapp.h"
#include "xbridgeexchange.h"
#include "util/util.h"
#include "util/logger.h"
#include "dht/dht.h"

#include <boost/asio.hpp>
#include <boost/asio/buffer.hpp>

//*****************************************************************************
//*****************************************************************************
XBridgeSession::XBridgeSession()
{
    // process this messages
    m_processors[xbcInvalid]               .bind(this, &XBridgeSession::processInvalid);

    // xchat message from wallet
    m_processors[xbcAnnounceAddresses]     .bind(this, &XBridgeSession::processAnnounceAddresses);

    // wallet received transaction
    m_processors[xbcReceivedTransaction]   .bind(this, &XBridgeSession::processBitcoinTransactionHash);

    // process transaction from client wallet
    m_processors[xbcTransaction]           .bind(this, &XBridgeSession::processTransaction);
    m_processors[xbcTransactionHoldApply]  .bind(this, &XBridgeSession::processTransactionHoldApply);
    // m_processors[xbcTransactionPayApply]   .bind(this, &XBridgeSession::processTransactionPayApply);
    // m_processors[xbcTransactionCommitApply].bind(this, &XBridgeSession::processTransactionCommitApply);
    m_processors[xbcTransactionCancel]     .bind(this, &XBridgeSession::processTransactionCancel);

    // retranslate messages to xbridge network
    m_processors[xbcXChatMessage]          .bind(this, &XBridgeSession::processXBridgeMessage);
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::processInvalid(XBridgePacketPtr /*packet*/)
{
    DEBUG_TRACE();
    LOG() << "xbcInvalid command processed";
    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::processAnnounceAddresses(XBridgePacketPtr packet)
{
    // DEBUG_TRACE();

    // size must be 20 bytes (160bit)
    if (packet->size() != 20)
    {
        ERR() << "invalid packet size for xbcAnnounceAddresses " << __FUNCTION__;
        return false;
    }

    XBridgeApp * app = qobject_cast<XBridgeApp *>(qApp);
    app->storageStore(shared_from_this(), packet->data());
    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::sendXBridgeMessage(const std::vector<unsigned char> & message)
{
    // DEBUG_TRACE();

    XBridgePacketPtr packet(new XBridgePacket());
    // packet->setData(message);
    packet->copyFrom(message);

    boost::system::error_code error;
    m_socket->send(boost::asio::buffer(packet->header(), packet->allSize()), 0, error);
    if (error)
    {
        ERR() << "packet send error " << PrintErrorCode(error) << __FUNCTION__;
        return false;
    }

    return false;
}

//*****************************************************************************
// retranslate packets from wallet to xbridge network
//*****************************************************************************
bool XBridgeSession::processXBridgeMessage(XBridgePacketPtr packet)
{
    DEBUG_TRACE();

    // size must be > 20 bytes (160bit)
    if (packet->size() <= 20)
    {
        ERR() << "invalid packet size for xbcXChatMessage " << __FUNCTION__;
        return false;
    }

    // read dest address
    std::vector<unsigned char> daddr(packet->data(), packet->data() + 20);

    XBridgeApp * app = qobject_cast<XBridgeApp *>(qApp);
    app->onSend(daddr, std::vector<unsigned char>(packet->header(), packet->header()+packet->allSize()));

    return true;
}

//*****************************************************************************
// retranslate packets from wallet to xbridge network
//*****************************************************************************
bool XBridgeSession::processXBridgeBroadcastMessage(XBridgePacketPtr packet)
{
    DEBUG_TRACE();

    XBridgeApp * app = qobject_cast<XBridgeApp *>(qApp);
    app->onSend(std::vector<unsigned char>(),
                std::vector<unsigned char>(packet->header(),
                                           packet->header()+packet->allSize()));

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::processTransaction(XBridgePacketPtr packet)
{
    // DEBUG_TRACE();

    // size must be 104 bytes
    if (packet->size() != 104)
    {
        ERR() << "invalid packet size for xbcTransaction " << __FUNCTION__;
        return false;
    }

    // check and process packet if bridge is exchange
    XBridgeExchange & e = XBridgeExchange::instance();
    if (e.isEnabled())
    {
        // read packet data
        uint256 id(packet->data());

        // source
        std::vector<unsigned char> saddr(packet->data()+32, packet->data()+52);
        std::string scurrency((const char *)packet->data()+52);
        boost::uint64_t samount = *static_cast<boost::uint64_t *>(static_cast<void *>(packet->data()+60));

        // destination
        std::vector<unsigned char> daddr(packet->data()+68, packet->data()+88);
        std::string dcurrency((const char *)packet->data()+88);
        boost::uint64_t damount = *static_cast<boost::uint64_t *>(static_cast<void *>(packet->data()+96));

        LOG() << "received transaction " << util::base64_encode(std::string((char *)id.begin(), 32)) << std::endl
              << "    from " << util::base64_encode(std::string((char *)&saddr[0], 20)) << std::endl
              << "             " << scurrency << " : " << samount << std::endl
              << "    to   " << util::base64_encode(std::string((char *)&daddr[0], 20)) << std::endl
              << "             " << dcurrency << " : " << damount << std::endl;

        if (!e.haveConnectedWallet(scurrency) || !e.haveConnectedWallet(dcurrency))
        {
            LOG() << "no active wallet for transaction " << util::base64_encode(std::string((char *)id.begin(), 32));
        }
        else
        {
            // float rate = (float) destAmount / sourceAmount;
            uint256 transactionId;
            if (e.createTransaction(id, saddr, scurrency, samount, daddr, dcurrency, damount, transactionId))
            {
                // check transaction state, if trNew - do nothing,
                // if trJoined = send hold to client
                XBridgeTransactionPtr tr = e.transaction(transactionId);
                if (tr && tr->state() == XBridgeTransaction::trJoined)
                {
                    // send hold to clients
                    XBridgeApp * app = qobject_cast<XBridgeApp *>(qApp);

                    // first
                    // TODO remove this log
                    LOG() << "send xbcTransactionHold to " << util::base64_encode(std::string((char *)&tr->firstAddress()[0], 20));

                    XBridgePacketPtr reply1(new XBridgePacket(xbcTransactionHold));
                    reply1->append(tr->firstAddress());
                    reply1->append(app->myid(), 20);
                    reply1->append(tr->firstId().begin(), 32);
                    reply1->append(transactionId.begin(), 32);

                    app->onSend(tr->firstAddress(),
                                std::vector<unsigned char>(reply1->header(),
                                                           reply1->header()+reply1->allSize()));

                    // second
                    // TODO remove this log
                    LOG() << "send xbcTransactionHold to " << util::base64_encode(std::string((char *)&tr->secondAddress()[0], 20));

                    XBridgePacketPtr reply2(new XBridgePacket(xbcTransactionHold));
                    reply2->append(tr->secondAddress());
                    reply2->append(app->myid(), 20);
                    reply2->append(tr->secondId().begin(), 32);
                    reply2->append(transactionId.begin(), 32);

                    app->onSend(tr->secondAddress(),
                                std::vector<unsigned char>(reply2->header(),
                                                           reply2->header()+reply2->allSize()));
                }
            }
        }
    }

    // ..and retranslate
    return processXBridgeBroadcastMessage(packet);
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::processTransactionHoldApply(XBridgePacketPtr packet)
{
    // DEBUG_TRACE();

    // size must be eq 52 bytes
    if (packet->size() != 52)
    {
        ERR() << "invalid packet size for xbcTransactionHoldApply " << __FUNCTION__;
        return false;
    }

    // check address
    XBridgeApp * app = qobject_cast<XBridgeApp *>(qApp);
    if (memcmp(packet->data(), app->myid(), 20) != 0)
    {
        // not for me, retranslate packet
        std::vector<unsigned char> addr(packet->data(), packet->data() + 20);
        app->onSend(addr, packet);
        return true;
    }

    XBridgeExchange & e = XBridgeExchange::instance();
    if (!e.isEnabled())
    {
        return true;
    }

    // transaction id
    uint256 id(packet->data()+20);

    if (e.updateTransactionWhenHoldApplyReceived(id))
    {
        XBridgeTransactionPtr tr = e.transaction(id);
        if (tr->state() == XBridgeTransaction::trHold)
        {
//            // send payment command to clients

//            // first
//            // TODO remove this log
//            LOG() << "send xbcTransactionPay to " << util::base64_encode(std::string((char *)&tr->firstAddress()[0], 20));

//            XBridgePacketPtr reply1(new XBridgePacket(xbcTransactionPay));
//            reply1->append(tr->firstAddress());
//            reply1->append(app->myid(), 20);
//            reply1->append(id.begin(), 32);
//            reply1->append(e.walletAddress(tr->firstCurrency()));

//            app->onSend(tr->firstAddress(),
//                        std::vector<unsigned char>(reply1->header(),
//                                                   reply1->header()+reply1->allSize()));

//            // second
//            // TODO remove this log
//            LOG() << "send xbcTransactionPay to " << util::base64_encode(std::string((char *)&tr->secondAddress()[0], 20));

//            XBridgePacketPtr reply2(new XBridgePacket(xbcTransactionPay));
//            reply2->append(tr->secondAddress());
//            reply2->append(app->myid(), 20);
//            reply2->append(id.begin(), 32);
//            reply2->append(e.walletAddress(tr->secondCurrency()));

//            app->onSend(tr->secondAddress(),
//                        std::vector<unsigned char>(reply2->header(),
//                                                   reply2->header()+reply2->allSize()));
        }
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
//bool XBridgeSession::processTransactionPayApply(XBridgePacketPtr packet)
//{
//    // DEBUG_TRACE();

//    // size must be 84 bytes
//    if (packet->size() != 84)
//    {
//        ERR() << "invalid packet size for xbcTransactionPayApply " << __FUNCTION__;
//        return false;
//    }

//    // check address
//    XBridgeApp * app = qobject_cast<XBridgeApp *>(qApp);
//    if (memcmp(packet->data(), app->myid(), 20) != 0)
//    {
//        // not for me, retranslate packet
//        std::vector<unsigned char> addr(packet->data(), packet->data() + 20);
//        app->onSend(addr, packet);
//        return true;
//    }

//    XBridgeExchange & e = XBridgeExchange::instance();
//    if (!e.isEnabled())
//    {
//        return true;
//    }

//    // transaction id
//    uint256 id(packet->data()+20);
//    uint256 paymentId(packet->data()+52);
//    if (e.updateTransactionWhenPayApplyReceived(id, paymentId))
//    {
//        XBridgeTransactionPtr tr = e.transaction(id);
//        if (tr->state() == XBridgeTransaction::trPaid)
//        {
//            // send commit payments from exchange wallets to client

//            {
//                // second-currency second-amount to first-destination
//                std::vector<unsigned char> walletAddress = e.walletAddress(tr->secondCurrency());

//                // TODO remove this log
//                LOG() << "send xbcTransactionCommit to "
//                      << util::base64_encode(std::string((char *)&walletAddress[0], 20));

//                XBridgePacketPtr reply(new XBridgePacket(xbcTransactionCommit));
//                reply->append(walletAddress);
//                reply->append(app->myid(), 20);
//                reply->append(id.begin(), 32);
//                reply->append(tr->firstDestination());
//                reply->append(tr->secondAmount());

//                app->onSend(walletAddress,
//                            std::vector<unsigned char>(reply->header(),
//                                                       reply->header()+reply->allSize()));
//            }

//            {
//                // first-currency first-amount to second-destination
//                std::vector<unsigned char> walletAddress = e.walletAddress(tr->firstCurrency());

//                // TODO remove this log
//                LOG() << "send xbcTransactionCommit to "
//                      << util::base64_encode(std::string((char *)&walletAddress[0], 20));

//                XBridgePacketPtr reply(new XBridgePacket(xbcTransactionCommit));
//                reply->append(walletAddress);
//                reply->append(app->myid(), 20);
//                reply->append(id.begin(), 32);
//                reply->append(tr->secondDestination());
//                reply->append(tr->firstAmount());

//                app->onSend(walletAddress,
//                            std::vector<unsigned char>(reply->header(),
//                                                       reply->header()+reply->allSize()));
//            }
//        }
//    }

//    return true;
//}

//*****************************************************************************
//*****************************************************************************
//bool XBridgeSession::processTransactionCommitApply(XBridgePacketPtr packet)
//{
//    // size must be 52 bytes
//    if (packet->size() != 52)
//    {
//        ERR() << "invalid packet size for xbcTransactionPayApply " << __FUNCTION__;
//        return false;
//    }

//    // check address
//    XBridgeApp * app = qobject_cast<XBridgeApp *>(qApp);
//    if (memcmp(packet->data(), app->myid(), 20) != 0)
//    {
//        // not for me, retranslate packet
//        std::vector<unsigned char> addr(packet->data(), packet->data() + 20);
//        app->onSend(addr, packet);
//        return true;
//    }

//    XBridgeExchange & e = XBridgeExchange::instance();
//    if (!e.isEnabled())
//    {
//        return true;
//    }

//    // transaction id
//    uint256 id(packet->data()+20);
//    if (e.updateTransactionWhenCommitApplyReceived(id))
//    {
//        XBridgeTransactionPtr tr = e.transaction(id);
//        if (tr->state() == XBridgeTransaction::trFinished)
//        {
//            // send transaction state to clients

//            // TODO remove this log
//            LOG() << "send xbcTransactionFinished to "
//                  << util::base64_encode(std::string((char *)&tr->firstAddress()[0], 20));

//            // first
//            XBridgePacketPtr reply1(new XBridgePacket(xbcTransactionFinished));
//            reply1->append(tr->firstAddress());
//            reply1->append(id.begin(), 32);

//            app->onSend(tr->firstAddress(),
//                        std::vector<unsigned char>(reply1->header(),
//                                                   reply1->header()+reply1->allSize()));

//            // TODO remove this log
//            LOG() << "send xbcTransactionFinished to "
//                  << util::base64_encode(std::string((char *)&tr->secondAddress()[0], 20));

//            // second
//            XBridgePacketPtr reply2(new XBridgePacket(xbcTransactionFinished));
//            reply2->append(tr->secondAddress());
//            reply2->append(id.begin(), 32);

//            app->onSend(tr->secondAddress(),
//                        std::vector<unsigned char>(reply2->header(),
//                                                   reply2->header()+reply2->allSize()));
//        }
//    }

//    return true;
//}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::processTransactionCancel(XBridgePacketPtr packet)
{
    // DEBUG_TRACE();

    // size must be == 52 bytes
    if (packet->size() != 52)
    {
        ERR() << "invalid packet size for xbcReceivedTransaction " << __FUNCTION__;
        return false;
    }

    XBridgeExchange & e = XBridgeExchange::instance();
    if (!e.isEnabled())
    {
        return true;
    }

    uint256 id(packet->data()+20);
    LOG() << "cancel transaction <" << id.GetHex() << ">";

    e.cancelTransaction(id);
    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeSession::processBitcoinTransactionHash(XBridgePacketPtr packet)
{
    // DEBUG_TRACE();

    // size must be == 32 bytes (256bit)
    if (packet->size() != 32)
    {
        ERR() << "invalid packet size for xbcReceivedTransaction " << __FUNCTION__;
        return false;
    }

    XBridgeExchange & e = XBridgeExchange::instance();
    if (!e.isEnabled())
    {
        return true;
    }

    uint256 id(packet->data());
    // LOG() << "received transaction <" << id.GetHex() << ">";

    e.updateTransaction(id);

    return true;
}
