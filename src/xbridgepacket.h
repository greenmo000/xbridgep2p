//******************************************************************************
//******************************************************************************

#ifndef XBRIDGEPACKET_H
#define XBRIDGEPACKET_H

#include <vector>
#include <deque>
#include <memory>
#include <ctime>
#include <boost/cstdint.hpp>
#include <boost/shared_ptr.hpp>

//******************************************************************************
//******************************************************************************
#define XBRIDGE_PROTOCOL_VERSION 0xff000005

//******************************************************************************
//******************************************************************************
enum XBridgeCommand
{
    xbcInvalid = 0,

    // client use this message for announce your addresses for network
    //
    // xbcAnnounceAddresses
    //     uint160 client address
    xbcAnnounceAddresses = 1,

    // xchat message
    //
    // xbcXChatMessage
    //     uint160 destination address
    //     serialized p2p message from bitcoin network
    xbcXChatMessage = 2,

    //      client1                   hub          client2
    // xbcTransaction           -->    |
    // xbcTransaction           -->    |
    // xbcTransaction           -->    |
    // xbcTransaction           -->    |     <-- xbcTransaction
    //                                 |
    // xbcTransactionHold       <--    |     --> xbcTransactionHold
    // xbcTransactionHoldApply  -->    |     <-- xbcTransactionHoldApply
    //                                 |
    // xbcTransactionCreate     <--    |     --> xbcTransactionCreate
    // xbcTransactionCreated    -->    |     <-- xbcTransactionCreated
    //                                 |
    // xbcTransactionSign       <--    |     --> xbcTransactionSign
    // xbcTransactionSigned     -->    |     <-- xbcTransactionSigned
    //                                 |
    // xbcTransactionCommit     <--    |     --> xbcTransactionCommit
    // xbcTransactionCommited   -->    |     <-- xbcTransactionCommited
    //                                 |
    //      hub wallet 1               |           hub wallet 2
    // xbcReceivedTransaction   -->    |     <-- xbcReceivedTransaction
    // xbcTransactionFinish     <--    |     --> xbcTransactionFinish


    // exchange transaction
    //
    // xbcTransaction
    // clients not process this messages, only exchange
    //    uint256 client transaction id
    //    uint160 source address
    //    8 bytes source currency
    //    uint64 source amount
    //    uint160 destination address
    //    8 bytes destination currency
    //    uint64 destination amount
    xbcTransaction = 3,
    //
    // xbcPendingTransaction (88 bytes)
    // exchange broadcast send this message, send list of opened transactions
    //    uint256 transaction id
    //    8 bytes source currency
    //    uint64 source amount
    //    8 bytes destination currency
    //    uint64 destination amount
    //    uint160 hub address
    //    uint32_t fee in percent, *1000 (0.3% == 300)
    xbcPendingTransaction = 4,
    //
    // xbcTransactionAccepting (124 bytes)
    // client accepting opened tx
    //    uint160 hub address
    //    uint256 client transaction id
    //    uint160 source address
    //    8 bytes source currency
    //    uint64 source amount
    //    uint160 destination address
    //    8 bytes destination currency
    //    uint64 destination amount
    xbcTransactionAccepting = 5,
    //
    // xbcTransactionHold
    //    uint160 client address
    //    uint160 hub address
    //    uint256 client transaction id
    //    uint256 hub transaction id
    xbcTransactionHold = 6,
    //
    // xbcTransactionHoldApply
    //    uint160 hub address
    //    uint160 client address
    //    uint256 hub transaction id
    xbcTransactionHoldApply = 7,
    //
    // xbcTransactionInit
    //    uint160 client address
    //    uint160 hub address
    //    uint256 hub transaction id
    //    uint160 source address
    //    8 bytes source currency
    //    uint64 source amount
    //    uint160 destination address
    //    8 bytes destination currency
    //    uint64 destination amount
    xbcTransactionInit = 8,
    //
    // xbcTransactionInitialized
    //    uint160 hub address
    //    uint160 client address
    //    uint256 hub transaction id
    xbcTransactionInitialized = 9,
    //
    // xbcTransactionCreate
    //    uint160  client address
    //    uint160  hub address
    //    uint256  hub transaction id
    //    uint160  destination address
    //    uint32_t tx1 lock time (in seconds)
    //    uint32_t tx2 lock time (in seconds)
    //    uint160  hub wallet address (for fee)
    //    uint32_t fee in percent, *1000 (0.3% == 300)
    xbcTransactionCreate = 10,
    //
    // xbcTransactionCreated
    //    uint160 hub address
    //    uint160 client address
    //    uint256 hub transaction id
    //    string  raw transaction
    xbcTransactionCreated = 11,
    //
    // xbcTransactionSign
    //    uint160 client address
    //    uint160 hub address
    //    uint256 hub transaction id
    //    string  raw transaction
    xbcTransactionSign = 12,
    //
    // xbcTransactionSigned
    //    uint160 hub address
    //    uint160 client address
    //    uint256 hub transaction id
    //    string  raw transaction (signed)
    xbcTransactionSigned = 13,
    //
    // xbcTransactionCommit
    //    uint160 hub address
    //    uint160 client address
    //    uint256 hub transaction id
    //    string  raw transaction (signed)
    xbcTransactionCommit = 14,
    //
    // xbcTransactionCommited
    //    uint160 hub address
    //    uint160 client address
    //    uint256 hub transaction id
    //    uint256 pay tx hash
    xbcTransactionCommited = 15,
    //
    // xbcTransactionConfirm
    //    uint160 client address
    //    uint160 hub address
    //    uint256 hub transaction id
    //    uint256 pay tx hash
    xbcTransactionConfirm = 16,
    //
    // xbcTransactionConfirmed
    //    uint160 hub address
    //    uint160 client address
    //    uint256 hub transaction id
    xbcTransactionConfirmed = 17,
    //
    // xbcTransactionCancel
    //    uint160 hub address
    //    uint256 hub transaction id
    xbcTransactionCancel = 18,
    //
    // xbcTransactionRollback
    //    uint160 hub address
    //    uint256 hub transaction id
    xbcTransactionRollback = 19,
    //
    // xbcTransactionFinished
    //    uint160 client address
    //    uint256 hub transaction id
    //
    xbcTransactionFinished = 20,
    //
    // xbcTransactionDropped
    //    uint160 address
    //    uint256 hub transaction id
    //
    xbcTransactionDropped = 21,

    // smart hub periodically send this message for invitations to trading
    // this message contains address of smart hub and
    // list of connected wallets (btc, xc, etc...)
    // broadcast message
    //
    // xbcExchangeWallets
    //     {wallet id (string)}|{wallet title (string)}|{wallet id (string)}|{wallet title (string)}
    xbcExchangeWallets = 22,

    // wallet send transaction hash when transaction received
    //
    // xbcReceivedTransaction
    //     uint256 transaction id (bitcoin transaction hash)
    xbcReceivedTransaction = 23,

    // address book entry
    //
    // xbcAddressBook
    xbcAddressBookEntry = 24
};

//******************************************************************************
//******************************************************************************
typedef boost::uint32_t crc_t;

//******************************************************************************
// header 8*4 bytes
//
// boost::uint32_t version
// boost::uint32_t command
// boost::uint32_t timestamp
// boost::uint32_t size
// boost::uint32_t crc
//
// boost::uint32_t rezerved
// boost::uint32_t rezerved
// boost::uint32_t rezerved
//******************************************************************************
class XBridgePacket
{
    std::vector<unsigned char> m_body;

public:
    enum
    {
        headerSize    = 8*sizeof(boost::uint32_t),
        commandSize   = sizeof(boost::uint32_t),
        timestampSize = sizeof(boost::uint32_t)
    };

    std::size_t     size()    const     { return sizeField(); }
    std::size_t     allSize() const     { return m_body.size(); }

    crc_t           crc()     const
    {
        // TODO implement this
        assert(false || "not implemented");
        return (0);
        // return crcField();
    }

    boost::uint32_t version() const       { return versionField(); }

    XBridgeCommand  command() const       { return static_cast<XBridgeCommand>(commandField()); }

    void    alloc()                       { m_body.resize(headerSize + size()); }

    const std::vector<unsigned char> & body() const
                                          { return m_body; }
    unsigned char  * header()             { return &m_body[0]; }
    unsigned char  * data()               { return &m_body[headerSize]; }

    // boost::int32_t int32Data() const { return field32<2>(); }

    void    clear()
    {
        m_body.resize(headerSize);
        commandField() = 0;
        sizeField() = 0;

        // TODO crc
        // crcField() = 0;
    }

    void resize(const unsigned int size)
    {
        m_body.resize(size+headerSize);
        sizeField() = size;
    }

    void    setData(const unsigned char data)
    {
        m_body.resize(sizeof(data) + headerSize);
        sizeField() = sizeof(data);
        m_body[headerSize] = data;
    }

    void    setData(const boost::int32_t data)
    {
        m_body.resize(sizeof(data) + headerSize);
        sizeField() = sizeof(data);
        field32<2>() = data;
    }

    void    setData(const std::string & data)
    {
        m_body.resize(data.size() + headerSize);
        sizeField() = data.size();
        if (data.size())
        {
            data.copy((char *)(&m_body[headerSize]), data.size());
        }
    }

    void    setData(const std::vector<unsigned char> & data, const unsigned int offset = 0)
    {
        setData(&data[0], data.size(), offset);
    }

    void    setData(const unsigned char * data, const unsigned int size, const unsigned int offset = 0)
    {
        unsigned int off = offset + headerSize;
        if (size)
        {
            if (m_body.size() < size+off)
            {
                m_body.resize(size+off);
                sizeField() = size+off-headerSize;
            }
            memcpy(&m_body[off], data, size);
        }
    }

    void append(const boost::uint32_t data)
    {
        m_body.reserve(m_body.size() + sizeof(data));
        unsigned char * ptr = (unsigned char *)&data;
        std::copy(ptr, ptr+sizeof(data), std::back_inserter(m_body));
        sizeField() = m_body.size() - headerSize;
    }

    void append(const boost::uint64_t data)
    {
        m_body.reserve(m_body.size() + sizeof(data));
        unsigned char * ptr = (unsigned char *)&data;
        std::copy(ptr, ptr+sizeof(data), std::back_inserter(m_body));
        sizeField() = m_body.size() - headerSize;
    }

    void append(const unsigned char * data, const int size)
    {
        m_body.reserve(m_body.size() + size);
        std::copy(data, data+size, std::back_inserter(m_body));
        sizeField() = m_body.size() - headerSize;
    }

    void append(const std::string & data)
    {
        m_body.reserve(m_body.size() + data.size()+1);
        std::copy(data.begin(), data.end(), std::back_inserter(m_body));
        m_body.push_back(0);
        sizeField() = m_body.size() - headerSize;
    }

    void append(const std::vector<unsigned char> & data)
    {
        m_body.reserve(m_body.size() + data.size());
        std::copy(data.begin(), data.end(), std::back_inserter(m_body));
        sizeField() = m_body.size() - headerSize;
    }

    void    copyFrom(const std::vector<unsigned char> & data)
    {
        m_body = data;

        if (sizeField() != data.size()-headerSize)
        {
            assert(false || "incorrect data size in XBridgePacket::copyFrom");
        }

        // TODO check packet crc
    }

    XBridgePacket() : m_body(headerSize, 0)
    {
        versionField()   = static_cast<boost::uint32_t>(XBRIDGE_PROTOCOL_VERSION);
        timestampField() = static_cast<boost::uint32_t>(time(0));
    }

    explicit XBridgePacket(const std::string& raw) : m_body(raw.begin(), raw.end())
    {
        timestampField() = static_cast<boost::uint32_t>(time(0));
    }

    XBridgePacket(const XBridgePacket & other)
    {
        m_body = other.m_body;
    }

    XBridgePacket(XBridgeCommand c) : m_body(headerSize, 0)
    {
        versionField()   = static_cast<boost::uint32_t>(XBRIDGE_PROTOCOL_VERSION);
        commandField()   = static_cast<boost::uint32_t>(c);
        timestampField() = static_cast<boost::uint32_t>(time(0));
    }

    XBridgePacket & operator = (const XBridgePacket & other)
    {
        m_body    = other.m_body;

        return *this;
    }

private:
    template<std::size_t INDEX>
    boost::uint32_t &       field32()
        { return *static_cast<boost::uint32_t *>(static_cast<void *>(&m_body[INDEX * 4])); }

    template<std::size_t INDEX>
    boost::uint32_t const& field32() const
        { return *static_cast<boost::uint32_t const*>(static_cast<void const*>(&m_body[INDEX * 4])); }

    boost::uint32_t       & versionField()         { return field32<0>(); }
    boost::uint32_t const & versionField() const   { return field32<0>(); }
    boost::uint32_t &       commandField()         { return field32<1>(); }
    boost::uint32_t const & commandField() const   { return field32<1>(); }
    boost::uint32_t &       timestampField()       { return field32<2>(); }
    boost::uint32_t const & timestampField() const { return field32<2>(); }
    boost::uint32_t &       sizeField()            { return field32<3>(); }
    boost::uint32_t const & sizeField() const      { return field32<3>(); }
    boost::uint32_t &       crcField()             { return field32<4>(); }
    boost::uint32_t const & crcField() const       { return field32<4>(); }
};

typedef boost::shared_ptr<XBridgePacket> XBridgePacketPtr;
typedef std::deque<XBridgePacketPtr>   XBridgePacketQueue;

#endif // XBRIDGEPACKET_H
