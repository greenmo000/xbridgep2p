//******************************************************************************
//******************************************************************************

#include "bitcoinrpctable.h"
#include "rpcstatuscode.h"
#include "sync.h"

#include <set>

//******************************************************************************
//******************************************************************************
namespace rpc
{

using namespace json_spirit;

//******************************************************************************
//******************************************************************************
std::string real_strprintf(const std::string &format, int dummy, ...);
#define strprintf(format, ...) real_strprintf(format, 0, __VA_ARGS__)
std::string vstrprintf(const char *format, va_list ap);

Object JSONRPCError(int code, const std::string & message);

//******************************************************************************
//******************************************************************************
Value help(const Array& params, bool fHelp);

//******************************************************************************
//******************************************************************************
static const RPCCommand rpcCommands[] =
{
    // name                      function
    {  "help",                   &help },
};

RPCTable rpcTable;

//******************************************************************************
//******************************************************************************
RPCTable::RPCTable()
{
    unsigned int vcidx;
    for (vcidx = 0; vcidx < (sizeof(rpcCommands) / sizeof(rpcCommands[0])); vcidx++)
    {
        const RPCCommand * pcmd = &rpcCommands[vcidx];
        mapCommands[pcmd->name] = pcmd;
    }
}

//******************************************************************************
//******************************************************************************
std::string RPCTable::help(std::string strCommand) const
{
    std::string strRet;
    std::set<rpcfn_type> setDone;
    for (std::map<std::string, const RPCCommand*>::const_iterator mi = mapCommands.begin();
         mi != mapCommands.end(); ++mi)
    {
        const RPCCommand *pcmd = mi->second;
        std::string strMethod = mi->first;
        // We already filter duplicates, but these deprecated screw up the sort order
        if (strMethod.find("label") != std::string::npos)
        {
            continue;
        }
        if (strCommand != "" && strMethod != strCommand)
        {
            continue;
        }
        try
        {
            Array params;
            rpcfn_type pfn = pcmd->actor;
            if (setDone.insert(pfn).second)
            {
                (*pfn)(params, true);
            }
        }
        catch (std::exception& e)
        {
            // Help text is returned in an exception
            std::string strHelp = std::string(e.what());
            if (strCommand == "")
            {
                if (strHelp.find('\n') != std::string::npos)
                {
                    strHelp = strHelp.substr(0, strHelp.find('\n'));
                }
            }
            strRet += strHelp + "\n";
        }
    }
    if (strRet == "")
    {
        strRet = strprintf("help: unknown command: %s\n", strCommand.c_str());
    }
    strRet = strRet.substr(0,strRet.size()-1);
    return strRet;
}

//******************************************************************************
//******************************************************************************
json_spirit::Value RPCTable::execute(const std::string &strMethod, const json_spirit::Array &params) const
{
    // Find method
    const RPCCommand *pcmd = rpcTable[strMethod];
    if (!pcmd)
    {
        throw JSONRPCError(RPC_METHOD_NOT_FOUND, "Method not found");
    }

    // Observe safe mode
//    std::string strWarning = GetWarnings("rpc");
//    if (strWarning != "" && !GetBoolArg("-disablesafemode") &&
//        !pcmd->okSafeMode)
//    {
//        throw JSONRPCError(RPC_FORBIDDEN_BY_SAFE_MODE, string("Safe mode: ") + strWarning);
//    }

    try
    {
        // Execute
        return pcmd->actor(params, false);
    }
    catch (std::exception& e)
    {
        throw JSONRPCError(RPC_MISC_ERROR, e.what());
    }
}

//******************************************************************************
//******************************************************************************
const RPCCommand * RPCTable::operator[](std::string name) const
{
    std::map<std::string, const RPCCommand*>::const_iterator it = mapCommands.find(name);
    if (it == mapCommands.end())
    {
        return nullptr;
    }
    return (*it).second;
}

} // namespace rpc
