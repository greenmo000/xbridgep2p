//******************************************************************************
//******************************************************************************

#ifndef RPCTABLE_H
#define RPCTABLE_H

#include "json/json_spirit_reader_template.h"
#include "json/json_spirit_writer_template.h"
#include "json/json_spirit_utils.h"

#include <string>
#include <map>

//******************************************************************************
//******************************************************************************
namespace rpc
{

//******************************************************************************
//******************************************************************************
typedef json_spirit::Value(*rpcfn_type)(const json_spirit::Array& params, bool fHelp);

//******************************************************************************
//******************************************************************************
class RPCCommand
{
public:
    std::string name;
    rpcfn_type actor;
};

//******************************************************************************
//******************************************************************************
class RPCTable
{
private:
    std::map<std::string, const RPCCommand *> mapCommands;
public:
    RPCTable();
    const RPCCommand* operator[](std::string name) const;
    std::string help(std::string name) const;

    /**
     * Execute a method.
     * @param method   Method to execute
     * @param params   Array of arguments (JSON objects)
     * @returns Result of the call.
     * @throws an exception (json_spirit::Value) when an error happens.
     */
    json_spirit::Value execute(const std::string &method, const json_spirit::Array &params) const;
};

} // namespace rpc

#endif // RPCTABLE_H
