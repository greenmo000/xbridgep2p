#ifndef RPCCOMMON_H
#define RPCCOMMON_H

#include "json/json_spirit_reader_template.h"
#include "json/json_spirit_writer_template.h"
#include "json/json_spirit_utils.h"

#include <string>

//*****************************************************************************
//*****************************************************************************
namespace rpc
{

using namespace json_spirit;

Object CallRPC(const std::string & rpcuser, const std::string & rpcpasswd,
               const std::string & rpcip, const std::string & rpcport,
               const std::string & strMethod, const Array & params);

} // namespace

#endif // RPCCOMMON_H
