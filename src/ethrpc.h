//******************************************************************************
//******************************************************************************

#ifndef ETHRPC_H
#define ETHRPC_H

#include <vector>
#include <string>
#include <cstdint>

//******************************************************************************
//******************************************************************************
namespace rpc {

// ethereum rpc
bool eth_gasPrice(const std::string & rpcip,
                  const std::string & rpcport,
                  uint64_t & gasPrice);
bool eth_accounts(const std::string & rpcip,
                  const std::string & rpcport,
                  std::vector<std::string> & addresses);
bool eth_getBalance(const std::string & rpcip,
                    const std::string & rpcport,
                    const std::string & account,
                    uint64_t & amount);
bool eth_sendTransaction(const std::string & rpcip,
                         const std::string & rpcport,
                         const std::string & from,
                         const std::string & to,
                         const uint64_t & amount,
                         const uint64_t & fee);

} // namespace rpc

#endif // ETHRPC_H
