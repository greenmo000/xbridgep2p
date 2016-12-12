
#include "json/json_spirit_reader_template.h"
#include "json/json_spirit_writer_template.h"
#include "json/json_spirit_utils.h"

#include <boost/asio.hpp>
//#include <boost/asio/ip/v6_only.hpp>
//#include <boost/bind.hpp>
#include <boost/filesystem.hpp>
#include <boost/iostreams/concepts.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/algorithm/string.hpp>
//#include <boost/lexical_cast.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/signals2.hpp>
//#include <list>
#include <stdio.h>
#include <atomic>

#include "bitcoinrpc.h"
#include "bignum.h"
#include "util/util.h"
#include "util/settings.h"
#include "util/logger.h"
#include "xbridgeapp.h"
#include "bitcoinrpcconnection.h"
#include "bitcoinrpcssliostreamdevice.h"

//*****************************************************************************
//*****************************************************************************
namespace rpc
{

using namespace json_spirit;
using namespace std;
using namespace boost;
using namespace boost::asio;

//*****************************************************************************
// HTTP status codes
//*****************************************************************************
enum HTTPStatusCode
{
    HTTP_OK                    = 200,
    HTTP_BAD_REQUEST           = 400,
    HTTP_UNAUTHORIZED          = 401,
    HTTP_FORBIDDEN             = 403,
    HTTP_NOT_FOUND             = 404,
    HTTP_INTERNAL_SERVER_ERROR = 500,
};

//******************************************************************************
//******************************************************************************
std::string real_strprintf(const std::string &format, int dummy, ...);
#define strprintf(format, ...) real_strprintf(format, 0, __VA_ARGS__)
std::string vstrprintf(const char *format, va_list ap);

//*****************************************************************************
//*****************************************************************************
string rfc1123Time()
{
    char buffer[64];
    time_t now;
    time(&now);
    struct tm* now_gmt = gmtime(&now);
    string locale(setlocale(LC_TIME, NULL));
    setlocale(LC_TIME, "C"); // we want POSIX (aka "C") weekday/month strings
    strftime(buffer, sizeof(buffer), "%a, %d %b %Y %H:%M:%S +0000", now_gmt);
    setlocale(LC_TIME, locale.c_str());
    return string(buffer);
}

//*****************************************************************************
//*****************************************************************************
std::string httpReply(int nStatus, const string& strMsg, bool keepalive)
{
    if (nStatus == HTTP_UNAUTHORIZED)
        return strprintf("HTTP/1.0 401 Authorization Required\r\n"
            "Date: %s\r\n"
            "Server: XCurrency-json-rpc/%s\r\n"
            "WWW-Authenticate: Basic realm=\"jsonrpc\"\r\n"
            "Content-Type: text/html\r\n"
            "Content-Length: 296\r\n"
            "\r\n"
            "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\"\r\n"
            "\"http://www.w3.org/TR/1999/REC-html401-19991224/loose.dtd\">\r\n"
            "<HTML>\r\n"
            "<HEAD>\r\n"
            "<TITLE>Error</TITLE>\r\n"
            "<META HTTP-EQUIV='Content-Type' CONTENT='text/html; charset=ISO-8859-1'>\r\n"
            "</HEAD>\r\n"
            "<BODY><H1>401 Unauthorized.</H1></BODY>\r\n"
            "</HTML>\r\n", rfc1123Time().c_str(), XBridgeApp::version().c_str());
    const char *cStatus;
         if (nStatus == HTTP_OK) cStatus = "OK";
    else if (nStatus == HTTP_BAD_REQUEST) cStatus = "Bad Request";
    else if (nStatus == HTTP_FORBIDDEN) cStatus = "Forbidden";
    else if (nStatus == HTTP_NOT_FOUND) cStatus = "Not Found";
    else if (nStatus == HTTP_INTERNAL_SERVER_ERROR) cStatus = "Internal Server Error";
    else cStatus = "";
    return strprintf(
            "HTTP/1.1 %d %s\r\n"
            "Date: %s\r\n"
            "Connection: %s\r\n"
            "Content-Length: %Iu\r\n"
            "Content-Type: application/json\r\n"
            "Server: XCurrency-json-rpc/%s\r\n"
            "\r\n"
            "%s",
        nStatus,
        cStatus,
        rfc1123Time().c_str(),
        keepalive ? "keep-alive" : "close",
        strMsg.size(),
        XBridgeApp::version().c_str(),
        strMsg.c_str());
}

//*****************************************************************************
//*****************************************************************************
bool clientAllowed(const boost::asio::ip::address & /*address*/)
{
    return true;
}

template <typename Protocol, typename SocketAcceptorService>
static void listen(boost::shared_ptr< basic_socket_acceptor<Protocol, SocketAcceptorService> > acceptor,
                      ssl::context & context,
                      const bool fUseSSL);

//*****************************************************************************
// Accept and handle incoming connection.
//*****************************************************************************
template <typename Protocol, typename SocketAcceptorService>
static void acceptHandler(boost::shared_ptr< basic_socket_acceptor<Protocol, SocketAcceptorService> > acceptor,
                          ssl::context & context,
                          const bool fUseSSL,
                          AcceptedConnection * conn,
                          const boost::system::error_code& error)
{
    // Immediately start accepting new connections, except when we're cancelled or our socket is closed.
    if (error != asio::error::operation_aborted && acceptor->is_open())
    {
        rpc::listen(acceptor, context, fUseSSL);
    }

    AcceptedConnectionImpl<ip::tcp>* tcp_conn = dynamic_cast< AcceptedConnectionImpl<ip::tcp>* >(conn);

    // TODO: Actually handle errors
    if (error)
    {
        delete conn;
    }

    // Restrict callers by IP.  It is important to
    // do this before starting client thread, to filter out
    // certain DoS and misbehaving clients.
    else if (tcp_conn && !clientAllowed(tcp_conn->peer.address()))
    {
        // Only send a 403 if we're not using SSL to prevent a DoS during the SSL handshake.
        if (!fUseSSL)
        {
            conn->stream() << httpReply(HTTP_FORBIDDEN, "", false) << std::flush;
        }
        delete conn;
    }

    // start HTTP client thread
    XBridgeApp & app = XBridgeApp::instance();
    app.handleRpcRequest(conn);
}

//*****************************************************************************
// Sets up I/O resources to accept and handle a new connection.
//*****************************************************************************
template <typename Protocol, typename SocketAcceptorService>
static void listen(boost::shared_ptr< basic_socket_acceptor<Protocol, SocketAcceptorService> > acceptor,
                      ssl::context & context,
                      const bool fUseSSL)
{
    // Accept connection
    AcceptedConnectionImpl<Protocol>* conn = new AcceptedConnectionImpl<Protocol>(acceptor->get_io_service(), context, fUseSSL);

    acceptor->async_accept(
            conn->sslStream.lowest_layer(),
            conn->peer,
            boost::bind(&acceptHandler<Protocol, SocketAcceptorService>,
                acceptor,
                boost::ref(context),
                fUseSSL,
                conn,
                boost::asio::placeholders::error));
}

//*****************************************************************************
//*****************************************************************************
bool httpAuthorized(map<string, string>& mapHeaders)
{
    string strAuth = mapHeaders["authorization"];
    if (strAuth.substr(0,6) != "Basic ")
    {
        return false;
    }
    string strUserPass64 = strAuth.substr(6);
    boost::trim(strUserPass64);
    string strUserPass = util::base64_decode(strUserPass64);

    static string strRPCUserColonPass = settings().rpcServerUserName() +
                                        ":" +
                                        settings().rpcServerPasswd();
    return strUserPass == strRPCUserColonPass;
}

//*****************************************************************************
//*****************************************************************************
void threadRPCServer()
{
    LOG() << "RPC server started";

    Settings & s = settings();
    std::string uname = s.rpcServerUserName();
    std::string passw = s.rpcServerPasswd();

    if (uname.size() == 0 || passw.size() == 0)
    {
        LOG() << "user name and password for rpc connection must be not empty";
        return;
    }

    asio::io_service io_service;

    const bool useSSL = s.rpcUseSsl();
    ssl::context context(io_service, ssl::context::sslv23);
    if (useSSL)
    {
        context.set_options(ssl::context::no_sslv2);

        filesystem::path pathCertFile(s.rpcSertFile("server.cert"));
        if (!pathCertFile.is_complete()) pathCertFile = filesystem::path(s.appPath()) / pathCertFile;
        if (filesystem::exists(pathCertFile)) context.use_certificate_chain_file(pathCertFile.string());
        else printf("ThreadRPCServer ERROR: missing server certificate file %s\n", pathCertFile.string().c_str());

        filesystem::path pathPKFile(s.rpcPKeyFile("server.pem"));
        if (!pathPKFile.is_complete()) pathPKFile = filesystem::path(s.appPath()) / pathPKFile;
        if (filesystem::exists(pathPKFile)) context.use_private_key_file(pathPKFile.string(), ssl::context::pem);
        else printf("ThreadRPCServer ERROR: missing server private key file %s\n", pathPKFile.string().c_str());

        string strCiphers = s.rpcSslCiphers("TLSv1+HIGH:!SSLv2:!aNULL:!eNULL:!AH:!3DES:@STRENGTH");
        SSL_CTX_set_cipher_list(context.impl(), strCiphers.c_str());
    }

    // Try a dual IPv6/IPv4 socket, falling back to separate IPv4 and IPv6 sockets
    // const bool loopback = !mapArgs.count("-rpcallowip");
    const bool loopback = false;
    asio::ip::address bindAddress = loopback ? asio::ip::address_v6::loopback() : asio::ip::address_v6::any();
    ip::tcp::endpoint endpoint(bindAddress, s.rpcPort(8080));
    boost::system::error_code v6_only_error;
    boost::shared_ptr<ip::tcp::acceptor> acceptor(new ip::tcp::acceptor(io_service));

    boost::signals2::signal<void ()> StopRequests;

    bool fListening = false;
    try
    {
        acceptor->open(endpoint.protocol());
        acceptor->set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));

        // Try making the socket dual IPv6/IPv4 (if listening on the "any" address)
        acceptor->set_option(boost::asio::ip::v6_only(loopback), v6_only_error);

        acceptor->bind(endpoint);
        acceptor->listen(socket_base::max_connections);

        listen(acceptor, context, useSSL);

        // Cancel outstanding listen-requests for this acceptor when shutting down
        StopRequests.connect(signals2::slot<void ()>(
                    static_cast<void (ip::tcp::acceptor::*)()>(&ip::tcp::acceptor::close), acceptor.get())
                .track(acceptor));

        fListening = true;
    }
    catch(boost::system::system_error &e)
    {
        ERR() << "An error occurred while setting up the RPC port "
              << endpoint.port()
              << " for listening on IPv6, falling back to IPv4: "
              << e.what();
    }

    try
    {
        // If dual IPv6/IPv4 failed (or we're opening loopback interfaces only), open IPv4 separately
        if (!fListening || loopback || v6_only_error)
        {
            bindAddress = loopback ? asio::ip::address_v4::loopback() : asio::ip::address_v4::any();
            endpoint.address(bindAddress);

            acceptor.reset(new ip::tcp::acceptor(io_service));
            acceptor->open(endpoint.protocol());
            acceptor->set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
            acceptor->bind(endpoint);
            acceptor->listen(socket_base::max_connections);

            listen(acceptor, context, useSSL);

            // Cancel outstanding listen-requests for this acceptor when shutting down
            StopRequests.connect(signals2::slot<void ()>(
                        static_cast<void (ip::tcp::acceptor::*)()>(&ip::tcp::acceptor::close), acceptor.get())
                    .track(acceptor));

            fListening = true;
        }
    }
    catch(boost::system::system_error &e)
    {
        ERR() << "An error occurred while setting up the RPC port "
              << endpoint.port()
              << " for listening on IPv4: "
              << e.what();
    }

    if (!fListening)
    {
        // uiInterface.ThreadSafeMessageBox(strerr, _("Error"), CClientUIInterface::OK | CClientUIInterface::MODAL);
        ERR() << "rpc server not started";
        return;
    }

    XBridgeApp & app = XBridgeApp::instance();
    while (!app.signalRpcStopActive())
    {
        io_service.run_one();
    }

    StopRequests();
}

} // namespace
