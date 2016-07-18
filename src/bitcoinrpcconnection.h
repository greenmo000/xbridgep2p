//*****************************************************************************
//*****************************************************************************

#ifndef ACCEPTEDCONNECTION_H
#define ACCEPTEDCONNECTION_H

#include "bitcoinrpcssliostreamdevice.h"

#include <string>
#include <iostream>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/iostreams/concepts.hpp>
#include <boost/iostreams/stream.hpp>

//*****************************************************************************
//*****************************************************************************
namespace rpc
{

//*****************************************************************************
//*****************************************************************************
class AcceptedConnection
{
public:
    virtual ~AcceptedConnection() {}

    virtual std::iostream& stream() = 0;
    virtual std::string peer_address_to_string() const = 0;
    virtual void close() = 0;
};

//*****************************************************************************
//*****************************************************************************
template <typename Protocol>
class AcceptedConnectionImpl : public AcceptedConnection
{
public:
    AcceptedConnectionImpl(
            boost::asio::io_service& io_service,
            boost::asio::ssl::context &context,
            bool fUseSSL) :
        sslStream(io_service, context),
        _d(sslStream, fUseSSL),
        _stream(_d)
    {
    }

    virtual std::iostream& stream()
    {
        return _stream;
    }

    virtual std::string peer_address_to_string() const
    {
        return peer.address().to_string();
    }

    virtual void close()
    {
        _stream.close();
    }

    typename Protocol::endpoint peer;
    boost::asio::ssl::stream<typename Protocol::socket> sslStream;

private:
    SSLIOStreamDevice<Protocol> _d;
    boost::iostreams::stream< SSLIOStreamDevice<Protocol> > _stream;
};

} // namespace

#endif // ACCEPTEDCONNECTION_H
