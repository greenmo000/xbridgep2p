//******************************************************************************
//******************************************************************************

#include "ethrpc.h"
#include "util/util.h"
#include "util/logger.h"

#include "rpccommon.h"

namespace rpc
{

//*****************************************************************************
//*****************************************************************************
bool eth_gasPrice(const std::string & rpcip,
                  const std::string & rpcport,
                  uint64_t & gasPrice)
{
    try
    {
        LOG() << "rpc call <eth_gasPrice>";

        Array params;
        Object reply = CallRPC("rpcuser", "rpcpasswd", rpcip, rpcport,
                               "eth_gasPrice", params);

        // Parse reply
        // const Value & result = find_value(reply, "result");
        const Value & error  = find_value(reply, "error");

        if (error.type() != null_type)
        {
            // Error
            LOG() << "error: " << write_string(error, false);
            // int code = find_value(error.get_obj(), "code").get_int();
            return false;
        }

        const Value & result = find_value(reply, "result");

        if (result.type() != str_type)
        {
            // Result
            LOG() << "result not an array " <<
                     (result.type() == null_type ? "" :
                       write_string(result, true));
            return false;
        }

        std::string value = result.get_str();
        gasPrice = strtoll(value.substr(2).c_str(), nullptr, 16);
    }
    catch (std::exception & e)
    {
        LOG() << "eth_accounts exception " << e.what();
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool eth_accounts(const std::string   & rpcip,
                  const std::string   & rpcport,
                  std::vector<std::string> & addresses)
{
    try
    {
        LOG() << "rpc call <eth_accounts>";

        Array params;
        Object reply = CallRPC("rpcuser", "rpcpasswd", rpcip, rpcport,
                               "eth_accounts", params);

        // Parse reply
        // const Value & result = find_value(reply, "result");
        const Value & error  = find_value(reply, "error");

        if (error.type() != null_type)
        {
            // Error
            LOG() << "error: " << write_string(error, false);
            // int code = find_value(error.get_obj(), "code").get_int();
            return false;
        }

        const Value & result = find_value(reply, "result");

        if (result.type() != array_type)
        {
            // Result
            LOG() << "result not an array " <<
                     (result.type() == null_type ? "" :
                      result.type() == str_type  ? result.get_str() :
                                                   write_string(result, true));
            return false;
        }

        Array arr = result.get_array();
        for (const Value & v : arr)
        {
            if (v.type() == str_type)
            {
                addresses.push_back(v.get_str());
            }
        }

    }
    catch (std::exception & e)
    {
        LOG() << "sendrawtransaction exception " << e.what();
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool eth_getBalance(const std::string & rpcip,
                    const std::string & rpcport,
                    const std::string & account,
                    uint64_t & amount)
{
    try
    {
        LOG() << "rpc call <eth_getBalance>";

//        std::vector<std::string> accounts;
//        rpc::eth_accounts(rpcip, rpcport, accounts);

//        for (const std::string & account : accounts)
        {
            Array params;
            params.push_back(account);
            params.push_back("latest");
            Object reply = CallRPC("rpcuser", "rpcpasswd", rpcip, rpcport,
                                   "eth_getBalance", params);

            // Parse reply
            // const Value & result = find_value(reply, "result");
            const Value & error  = find_value(reply, "error");

            if (error.type() != null_type)
            {
                // Error
                LOG() << "error: " << write_string(error, false);
                // int code = find_value(error.get_obj(), "code").get_int();
                return false;
            }

            const Value & result = find_value(reply, "result");

            if (result.type() != str_type)
            {
                // Result
                LOG() << "result not an string " <<
                         (result.type() == null_type ? "" :
                          write_string(result, true));
                return false;
            }

            std::string value = result.get_str();
            amount = strtoll(value.substr(2).c_str(), nullptr, 16);
        }
    }
    catch (std::exception & e)
    {
        LOG() << "sendrawtransaction exception " << e.what();
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool eth_sendTransaction(const std::string & rpcip,
                         const std::string & rpcport,
                         const std::string & from,
                         const std::string & to,
                         const uint64_t & amount,
                         const uint64_t & /*fee*/)
{
    try
    {
        LOG() << "rpc call <eth_sendTransaction>";

        Array params;
        // params.push_back(rawtx);

        Object o;
        o.push_back(Pair("from",       from));
        o.push_back(Pair("to",         to));
        o.push_back(Pair("gas",        "0x76c0"));
        o.push_back(Pair("gasPrice",   "0x9184e72a000"));
        // o.push_back(Pair("value",      "0x9184e72a"));

        char buf[64];
        sprintf(buf, "%ullx", amount);
        o.push_back(Pair("value", buf));

        // o.push_back(Pair("data",       "0xd46e8dd67c5d32be8d46e8dd67c5d32be8058bb8eb970870f072445675058bb8eb970870f072445675"));

        params.push_back(o);

        Object reply = CallRPC("rpcuser", "rpcpasswd", rpcip, rpcport,
                               "eth_sendTransaction", params);

        // Parse reply
        // const Value & result = find_value(reply, "result");
        const Value & error  = find_value(reply, "error");

        if (error.type() != null_type)
        {
            // Error
            LOG() << "error: " << write_string(error, false);
            // int code = find_value(error.get_obj(), "code").get_int();
            return false;
        }
    }
    catch (std::exception & e)
    {
        LOG() << "sendrawtransaction exception " << e.what();
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool eth_sign(const std::string & rpcip,
              const std::string & rpcport,
              const std::string & addr,
              const std::string & data,
              std::string & result)
{
    try
    {
        LOG() << "rpc call <eth_sign>";

        Array params;
        params.push_back(addr);
        params.push_back(data);

        Object reply = CallRPC("rpcuser", "rpcpasswd", rpcip, rpcport,
                               "eth_sign", params);

        // Parse reply
        // const Value & result = find_value(reply, "result");
        const Value & error  = find_value(reply, "error");

        if (error.type() != null_type)
        {
            // Error
            LOG() << "error: " << write_string(error, false);
            // int code = find_value(error.get_obj(), "code").get_int();
            return false;
        }

        const Value & vresult = find_value(reply, "result");

        if (vresult.type() != str_type)
        {
            // Result
            LOG() << "result not an string " <<
                     (vresult.type() == null_type ? "" :
                      write_string(vresult, true));
            return false;
        }

        result = vresult.get_str();
    }
    catch (std::exception & e)
    {
        LOG() << "sendrawtransaction exception " << e.what();
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool eth_sendRawTransaction(const std::string & rpcip,
                            const std::string & rpcport,
                            const std::string & tx)
{
    try
    {
        LOG() << "rpc call <eth_sendRawTransaction>";

        Array params;
        params.push_back(tx);

        Object reply = CallRPC("rpcuser", "rpcpasswd", rpcip, rpcport,
                               "eth_sendRawTransaction", params);

        // Parse reply
        // const Value & result = find_value(reply, "result");
        const Value & error  = find_value(reply, "error");

        if (error.type() != null_type)
        {
            // Error
            LOG() << "error: " << write_string(error, false);
            // int code = find_value(error.get_obj(), "code").get_int();
            return false;
        }
    }
    catch (std::exception & e)
    {
        LOG() << "sendrawtransaction exception " << e.what();
        return false;
    }

    return true;
}

} // namespace rpc
