//*****************************************************************************
//*****************************************************************************

// #include <boost/asio.hpp>
// #include <boost/asio/buffer.hpp>
#include <boost/algorithm/string.hpp>

#include "xbridgesessionethereum.h"
#include "xbridgeapp.h"
#include "xbridgeexchange.h"
#include "uiconnector.h"
#include "util/util.h"
#include "util/logger.h"
#include "util/txlog.h"
#include "dht/dht.h"
#include "bitcoinrpc.h"
#include "ctransaction.h"
#include "base58.h"

//*****************************************************************************
//*****************************************************************************
XBridgeSessionEthereum::XBridgeSessionEthereum()
{
    init();
}

//*****************************************************************************
//*****************************************************************************
XBridgeSessionEthereum::XBridgeSessionEthereum(const WalletParam & wallet)
    : XBridgeSession(wallet)
{
    // m_wallet.COIN = 1000000000000000000;

    init();
}

//*****************************************************************************
//*****************************************************************************
XBridgeSessionEthereum::~XBridgeSessionEthereum()
{

}

//*****************************************************************************
//*****************************************************************************
void XBridgeSessionEthereum::init()
{
    assert(!m_handlers.size());

    dht_random_bytes(m_myid, sizeof(m_myid));
    LOG() << "session <" << m_wallet.currency << "> generated id <"
             << util::base64_encode(std::string((char *)m_myid, sizeof(m_myid))).c_str()
             << ">";

    // process invalid
    m_handlers[xbcInvalid]               .bind(this, &XBridgeSessionEthereum::processInvalid);

    m_handlers[xbcAnnounceAddresses]     .bind(this, &XBridgeSessionEthereum::processAnnounceAddresses);

    // process transaction from client wallet
    // if (XBridgeExchange::instance().isEnabled())
    {
        m_handlers[xbcTransaction]           .bind(this, &XBridgeSessionEthereum::processTransaction);
        m_handlers[xbcTransactionAccepting]   .bind(this, &XBridgeSessionEthereum::processTransactionAccepting);
    }
    // else
    {
        m_handlers[xbcPendingTransaction]    .bind(this, &XBridgeSessionEthereum::processPendingTransaction);
    }

    // transaction processing
    {
        m_handlers[xbcTransactionHold]       .bind(this, &XBridgeSessionEthereum::processTransactionHold);
        m_handlers[xbcTransactionHoldApply]  .bind(this, &XBridgeSessionEthereum::processTransactionHoldApply);

        m_handlers[xbcTransactionInit]       .bind(this, &XBridgeSessionEthereum::processTransactionInit);
        m_handlers[xbcTransactionInitialized].bind(this, &XBridgeSessionEthereum::processTransactionInitialized);

        m_handlers[xbcTransactionCreate]     .bind(this, &XBridgeSessionEthereum::processTransactionCreate);
        m_handlers[xbcTransactionCreated]    .bind(this, &XBridgeSessionEthereum::processTransactionCreated);

//        m_handlers[xbcTransactionSignRefund]       .bind(this, &XBridgeSessionEthereum::processTransactionSignRefund);
//        m_handlers[xbcTransactionRefundSigned]     .bind(this, &XBridgeSessionEthereum::processTransactionRefundSigned);

//        m_handlers[xbcTransactionCommitStage1]     .bind(this, &XBridgeSessionEthereum::processTransactionCommitStage1);
//        m_handlers[xbcTransactionCommitedStage1]   .bind(this, &XBridgeSessionEthereum::processTransactionCommitedStage1);

        m_handlers[xbcTransactionConfirm]    .bind(this, &XBridgeSessionEthereum::processTransactionConfirm);

        m_handlers[xbcTransactionCancel]     .bind(this, &XBridgeSessionEthereum::processTransactionCancel);
        m_handlers[xbcTransactionRollback]   .bind(this, &XBridgeSessionEthereum::processTransactionRollback);
        m_handlers[xbcTransactionFinished]   .bind(this, &XBridgeSessionEthereum::processTransactionFinished);
        m_handlers[xbcTransactionDropped]    .bind(this, &XBridgeSessionEthereum::processTransactionDropped);

        m_handlers[xbcTransactionConfirmed]  .bind(this, &XBridgeSessionEthereum::processTransactionConfirmed);

        // wallet received transaction
        m_handlers[xbcReceivedTransaction]   .bind(this, &XBridgeSessionEthereum::processBitcoinTransactionHash);
    }

    m_handlers[xbcAddressBookEntry].bind(this, &XBridgeSessionEthereum::processAddressBookEntry);

    // retranslate messages to xbridge network
    m_handlers[xbcXChatMessage].bind(this, &XBridgeSessionEthereum::processXChatMessage);
}

//******************************************************************************
//******************************************************************************
bool XBridgeSessionEthereum::processTransactionCreate(XBridgePacketPtr packet)
{
    assert(!"not implemented");
    return true;

//    DEBUG_TRACE_LOG(currencyToLog());

//    if (packet->size() != 124)
//    {
//        ERR() << "incorrect packet size for xbcTransactionCreate" << __FUNCTION__;
//        return false;
//    }

//    std::vector<unsigned char> thisAddress(packet->data(), packet->data()+20);
//    std::vector<unsigned char> hubAddress(packet->data()+20, packet->data()+40);

//    // transaction id
//    uint256 id   (packet->data()+40);

//    // destination address
//    std::vector<unsigned char> destAddress(packet->data()+72, packet->data()+92);

//    // tax
//    std::vector<unsigned char> taxAddress(packet->data()+100, packet->data()+120);
//    const uint32_t tax = *reinterpret_cast<uint32_t*>(packet->data()+120);

//    XBridgeTransactionDescrPtr xtx;
//    {
//        boost::mutex::scoped_lock l(XBridgeApp::m_txLocker);

//        if (!XBridgeApp::m_transactions.count(id))
//        {
//            // wtf? unknown transaction
//            // TODO log
//            return false;
//        }

//        xtx = XBridgeApp::m_transactions[id];
//    }

//    // lock time
//    xtx->lockTimeTx1 = *reinterpret_cast<boost::uint32_t *>(packet->data()+92);
//    xtx->lockTimeTx2 = *reinterpret_cast<boost::uint32_t *>(packet->data()+96);

//    uint64_t amount = 0;
//    if (!rpc::eth_getBalance(m_wallet.ip, m_wallet.port, m_wallet.address, amount))
//    {
//        LOG() << "rpc::eth_getBalance failed" << __FUNCTION__;
//        return false;
//    }

////    uint64_t gasPrice = 0;
////    if (!rpc::eth_gasPrice(m_address, m_port, gasPrice))
////    {
////        LOG() << "rpc::eth_getBalance failed" << __FUNCTION__;
////        return false;
////    }

//    boost::uint64_t outAmount = m_wallet.COIN*(static_cast<double>(xtx->fromAmount)/XBridgeTransactionDescr::COIN)+m_fee;
//    boost::uint64_t taxAmount = m_wallet.COIN*(static_cast<double>(xtx->fromAmount*tax/100000)/XBridgeTransactionDescr::COIN);

//    boost::uint64_t fee = 0;
//    boost::uint64_t inAmount  = 0;

//    // check amount
//    if (amount < outAmount)
//    {
//        // no money, cancel transaction
//        sendCancelTransaction(id, crNoMoney);
//        return false;
//    }

//    xtx->state = XBridgeTransactionDescr::trCreated;
//    uiConnector.NotifyXBridgeTransactionStateChanged(id, xtx->state);

//    // send reply
//    XBridgePacketPtr reply(new XBridgePacket(xbcTransactionCreated));
//    reply->append(hubAddress);
//    reply->append(thisAddress);
//    reply->append(id.begin(), 32);
//    reply->append("ETHEREUM_TRANSACTION");
//    reply->append("ETHEREUM_TRANSACTION");

//    sendPacket(hubAddress, reply);
//    return true;
}

//******************************************************************************
//******************************************************************************
bool XBridgeSessionEthereum::processTransactionSignRefund(XBridgePacketPtr packet)
{
    DEBUG_TRACE_LOG(currencyToLog());

    if (packet->size() < 72)
    {
        ERR() << "incorrect packet size for xbcTransactionSign" << __FUNCTION__;
        return false;
    }

    std::vector<unsigned char> thisAddress(packet->data(), packet->data()+20);

    size_t offset = 20;
    std::vector<unsigned char> hubAddress(packet->data()+offset, packet->data()+offset+20);
    offset += 20;

    uint256 txid(packet->data()+offset);
    offset += 32;

    std::string rawtxpay(reinterpret_cast<const char *>(packet->data()+offset));
    offset += rawtxpay.size()+1;

    std::string rawtxrev(reinterpret_cast<const char *>(packet->data()+offset));

    // check txid
    XBridgeTransactionDescrPtr xtx;
    {
        boost::mutex::scoped_lock l(XBridgeApp::m_txLocker);

        if (!XBridgeApp::m_transactions.count(txid))
        {
            // wtf? unknown transaction
            // TODO log
            return false;
        }

        xtx = XBridgeApp::m_transactions[txid];
    }

    xtx->state = XBridgeTransactionDescr::trSigned;
    uiConnector.NotifyXBridgeTransactionStateChanged(txid, xtx->state);

    // send reply
//    XBridgePacketPtr reply(new XBridgePacket(xbcTransactionRefundSigned));
//    reply->append(hubAddress);
//    reply->append(thisAddress);
//    reply->append(txid.begin(), 32);
//    reply->append(rawtxrev);

//    sendPacket(hubAddress, reply);
    return true;
}

//******************************************************************************
//******************************************************************************
bool XBridgeSessionEthereum::processTransactionCommitStage1(XBridgePacketPtr packet)
{
    assert(!"not implemented");
    return true;

//    DEBUG_TRACE_LOG(currencyToLog());

//    if (packet->size() < 72)
//    {
//        ERR() << "incorrect packet size for xbcTransactionCommit" << __FUNCTION__;
//        return false;
//    }

//    std::vector<unsigned char> thisAddress(packet->data(), packet->data()+20);
//    std::vector<unsigned char> hubAddress(packet->data()+20, packet->data()+40);

//    uint256 txid(packet->data()+40);

//    // std::string rawtx(reinterpret_cast<const char *>(packet->data()+72));

//    // send pay transaction to network
//    XBridgeTransactionDescrPtr xtx;
//    {
//        boost::mutex::scoped_lock l(XBridgeApp::m_txLocker);

//        if (!XBridgeApp::m_transactions.count(txid))
//        {
//            // wtf? unknown transaction
//            // TODO log
//            return false;
//        }

//        xtx = XBridgeApp::m_transactions[txid];
//    }

//    std::string from = "0x" + std::string(reinterpret_cast<char *>(&xtx->from[0]), 20);
//    std::string to   = "0x" + std::string(reinterpret_cast<char *>(&xtx->to[0]), 20);

//    uint64_t outAmount = m_wallet.COIN*(static_cast<double>(xtx->fromAmount)/XBridgeTransactionDescr::COIN);
//    if (!rpc::eth_sendTransaction(m_wallet.ip, m_wallet.port, from, to, outAmount, m_fee))
//    {
//        // not commited....send cancel???
//        // sendCancelTransaction(id);
//        return false;
//    }

//    xtx->state = XBridgeTransactionDescr::trCommited;
//    uiConnector.NotifyXBridgeTransactionStateChanged(txid, xtx->state);

//    assert(!"not finished");

//    // send commit apply to hub
//    XBridgePacketPtr reply(new XBridgePacket(xbcTransactionCommitedStage1));
//    reply->append(hubAddress);
//    reply->append(thisAddress);
//    reply->append(txid.begin(), 32);
//    // reply->append(xtx->payTxId.begin(), 32);

//    sendPacket(hubAddress, reply);
//    return true;
}

//*****************************************************************************
//*****************************************************************************
std::vector<unsigned char> toXBridgeAddr(const std::string & etherAddress)
{
    std::string waddr = etherAddress.substr(2);

    assert(etherAddress.size() == 42 && "incorrect address length");
    assert(etherAddress[0] == '0' && etherAddress[1] == 'x' && "incorrect address prefix");
    assert(waddr.size() == 40 && "incorrect address length");

    std::vector<unsigned char> ucaddr;

    if (waddr.size() == 40)
    {
        char ch[4];
        memset(ch, 0, 4);

        for (size_t i = 0; i < waddr.size(); i += 2)
        {
            ch[0] = waddr[i];
            ch[1] = waddr[i+1];
            int a = strtol(ch, 0, 16);
            ucaddr.push_back(a);
        }
    }

    return ucaddr;
}

//*****************************************************************************
//*****************************************************************************
void XBridgeSessionEthereum::requestAddressBook()
{
//    std::vector<std::string> addrs;
//    if (!rpc::eth_accounts(m_address, m_port, addrs))
//    {
//        return;
//    }

//    XBridgeApp & app = XBridgeApp::instance();

//    for (const std::string & addr : addrs)
//    {
//        app.storageStore(shared_from_this(), reinterpret_cast<const unsigned char *>(addr.substr(2).c_str()));

//        uiConnector.NotifyXBridgeAddressBookEntryReceived
//                (m_currency, "ETHEREUM", util::base64_encode(addr.substr(2)));
//    }

    std::vector<unsigned char> addr = toXBridgeAddr(m_wallet.address);

    XBridgeApp & app = XBridgeApp::instance();
    app.storageStore(shared_from_this(), &addr[0]);

    uiConnector.NotifyXBridgeAddressBookEntryReceived
            (m_wallet.currency, "ETHEREUM", util::base64_encode(addr));
}

//******************************************************************************
//******************************************************************************
bool XBridgeSessionEthereum::revertXBridgeTransaction(const uint256 & id)
{
    DEBUG_TRACE_LOG(currencyToLog());

    // TODO temporary implementation
    XBridgeTransactionDescrPtr xtx;
    {
        boost::mutex::scoped_lock l(XBridgeApp::m_txLocker);

        // search tx
        for (std::map<uint256, XBridgeTransactionDescrPtr>::iterator i = XBridgeApp::m_transactions.begin();
             i != XBridgeApp::m_transactions.end(); ++i)
        {
            if (i->second->id == id)
            {
                xtx = i->second;
                break;
            }
        }
    }

    if (!xtx)
    {
        return false;
    }

//    // rollback, commit revert transaction
//    if (!rpc::sendRawTransaction(m_user, m_passwd, m_address, m_port, xtx->revTx))
//    {
//        // not commited....send cancel???
//        // sendCancelTransaction(id);
//        return false;
//    }

    return true;
}
