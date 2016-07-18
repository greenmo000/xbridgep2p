//******************************************************************************
//******************************************************************************

#include "json/json_spirit_reader_template.h"
#include "json/json_spirit_writer_template.h"
#include "json/json_spirit_utils.h"

#include <boost/asio.hpp>
//#include <boost/asio/ip/v6_only.hpp>
//#include <boost/bind.hpp>
#include <boost/filesystem.hpp>
#include <boost/foreach.hpp>
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

#include "bignum.h"
#include "util/util.h"
#include "util/settings.h"
#include "util/logger.h"
#include "xbridgeapp.h"
#include "bitcoinrpc.h"
#include "bitcoinrpctable.h"
#include "bitcoinrpcconnection.h"
#include "bitcoinrpcssliostreamdevice.h"
#include "httpstatuscode.h"
#include "rpcstatuscode.h"

//*****************************************************************************
//*****************************************************************************
namespace rpc
{

using namespace json_spirit;
using namespace std;
using namespace boost;
using namespace boost::asio;

extern std::atomic<bool> stopRpc;
extern RPCTable rpcTable;

int readHTTP(std::basic_istream<char> & stream,
             std::map<std::string, std::string> & mapHeadersRet,
             std::string & strMessageRet);
std::string httpReply(int nStatus, const std::string & strMsg, bool keepalive);
bool httpAuthorized(map<string, string>& mapHeaders);

static Object JSONRPCExecOne(const Value& req);

//******************************************************************************
//******************************************************************************
class JSONRequest
{
public:
    Value id;
    string strMethod;
    Array params;

    JSONRequest() { id = Value::null; }
    void parse(const Value& valRequest);
};

//******************************************************************************
//******************************************************************************
Object JSONRPCError(int code, const string& message)
{
    Object error;
    error.push_back(Pair("code", code));
    error.push_back(Pair("message", message));
    return error;
}

//******************************************************************************
//******************************************************************************
Object JSONRPCReplyObj(const Value& result, const Value& error, const Value& id)
{
    Object reply;
    if (error.type() != null_type)
        reply.push_back(Pair("result", Value::null));
    else
        reply.push_back(Pair("result", result));
    reply.push_back(Pair("error", error));
    reply.push_back(Pair("id", id));
    return reply;
}

//******************************************************************************
//******************************************************************************
string JSONRPCReply(const Value& result, const Value& error, const Value& id)
{
    Object reply = JSONRPCReplyObj(result, error, id);
    return write_string(Value(reply), false) + "\n";
}

//******************************************************************************
//******************************************************************************
static string JSONRPCExecBatch(const Array& vReq)
{
    Array ret;
    for (unsigned int reqIdx = 0; reqIdx < vReq.size(); reqIdx++)
        ret.push_back(JSONRPCExecOne(vReq[reqIdx]));

    return write_string(Value(ret), false) + "\n";
}

//******************************************************************************
//******************************************************************************
void errorReply(std::ostream& stream, const Object& objError, const Value& id)
{
    // Send error reply from json-rpc error object
    int nStatus = HTTP_INTERNAL_SERVER_ERROR;
    int code = find_value(objError, "code").get_int();
    if (code == RPC_INVALID_REQUEST) nStatus = HTTP_BAD_REQUEST;
    else if (code == RPC_METHOD_NOT_FOUND) nStatus = HTTP_NOT_FOUND;
    string strReply = JSONRPCReply(Value::null, objError, id);
    stream << httpReply(nStatus, strReply, false) << std::flush;
}

//******************************************************************************
//******************************************************************************
void JSONRequest::parse(const Value& valRequest)
{
    // Parse request
    if (valRequest.type() != obj_type)
    {
        throw JSONRPCError(RPC_INVALID_REQUEST, "Invalid Request object");
    }
    const Object& request = valRequest.get_obj();

    // Parse id now so errors from here on will have the id
    id = find_value(request, "id");

    // Parse method
    Value valMethod = find_value(request, "method");
    if (valMethod.type() == null_type)
    {
        throw JSONRPCError(RPC_INVALID_REQUEST, "Missing method");
    }
    if (valMethod.type() != str_type)
    {
        throw JSONRPCError(RPC_INVALID_REQUEST, "Method must be a string");
    }
    strMethod = valMethod.get_str();
    if (strMethod != "getwork" && strMethod != "getblocktemplate")
    {
        printf("ThreadRPCServer method=%s\n", strMethod.c_str());
    }

    // Parse params
    Value valParams = find_value(request, "params");
    if (valParams.type() == array_type)
    {
        params = valParams.get_array();
    }
    else if (valParams.type() == null_type)
    {
        params = Array();
    }
    else
    {
        throw JSONRPCError(RPC_INVALID_REQUEST, "Params must be an array");
    }
}

//******************************************************************************
//******************************************************************************
static Object JSONRPCExecOne(const Value& req)
{
    Object rpc_result;

    JSONRequest jreq;
    try {
        jreq.parse(req);

        Value result = rpcTable.execute(jreq.strMethod, jreq.params);
        rpc_result = JSONRPCReplyObj(result, Value::null, jreq.id);
    }
    catch (Object& objError)
    {
        rpc_result = JSONRPCReplyObj(Value::null, objError, jreq.id);
    }
    catch (std::exception& e)
    {
        rpc_result = JSONRPCReplyObj(Value::null,
                                     JSONRPCError(RPC_PARSE_ERROR, e.what()), jreq.id);
    }

    return rpc_result;
}

//******************************************************************************
//******************************************************************************
void handleRpcRequest(AcceptedConnection * conn)
{
    bool fRun = true;
    while(true)
    {
        if (stopRpc || !fRun)
        {
            conn->close();
            delete conn;
            return;
        }

        std::map<std::string, std::string> mapHeaders;
        std::string strRequest;

        readHTTP(conn->stream(), mapHeaders, strRequest);

        // Check authorization
        if (mapHeaders.count("authorization") == 0)
        {
            conn->stream() << httpReply(HTTP_UNAUTHORIZED, "", false) << std::flush;
            break;
        }
        if (!httpAuthorized(mapHeaders))
        {
            LOG() << "ThreadRPCServer incorrect password attempt from "
                  << conn->peer_address_to_string().c_str();

            // Deter brute-forcing short passwords.
            // If this results in a DOS the user really
            // shouldn't have their RPC port exposed
            static bool isShortPassword = settings().rpcServerPasswd().size() < 20;
            if (isShortPassword)
            {
                Sleep(250);
            }

            conn->stream() << httpReply(HTTP_UNAUTHORIZED, "", false) << std::flush;
            break;
        }

        if (mapHeaders["connection"] == "close")
        {
            fRun = false;
        }

        JSONRequest jreq;
        try
        {
            // Parse request
            Value valRequest;
            if (!read_string(strRequest, valRequest))
                throw JSONRPCError(RPC_PARSE_ERROR, "Parse error");

            string strReply;

            // singleton request
            if (valRequest.type() == obj_type) {
                jreq.parse(valRequest);

                Value result = rpcTable.execute(jreq.strMethod, jreq.params);

                // Send reply
                strReply = JSONRPCReply(result, Value::null, jreq.id);

            // array of requests
            } else if (valRequest.type() == array_type)
                strReply = JSONRPCExecBatch(valRequest.get_array());
            else
                throw JSONRPCError(RPC_PARSE_ERROR, "Top-level object parse error");

            conn->stream() << httpReply(HTTP_OK, strReply, fRun) << std::flush;
        }
        catch (Object& objError)
        {
            errorReply(conn->stream(), objError, jreq.id);
            break;
        }
        catch (std::exception& e)
        {
            errorReply(conn->stream(), JSONRPCError(RPC_PARSE_ERROR, e.what()), jreq.id);
            break;
        }
    }

    delete conn;
}

//******************************************************************************
//******************************************************************************
Value help(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
            "help [command]\n"
            "List commands, or get help for a command.");

    string strCommand;
    if (params.size() > 0)
        strCommand = params[0].get_str();

    return rpcTable.help(strCommand);
}

} // namespace rpc