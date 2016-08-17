//******************************************************************************
//******************************************************************************

#ifndef _BITCOINRPC_H_
#define _BITCOINRPC_H_

#include <vector>
#include <string>
#include <cstdint>

//******************************************************************************
//******************************************************************************
namespace rpc
{
    void threadRPCServer();

    class AcceptedConnection;
    void handleRpcRequest(AcceptedConnection * conn);

    bool DecodeBase58Check(const char * psz, std::vector<unsigned char> & vchRet);

    typedef std::pair<std::string, std::vector<std::string> > AddressBookEntry;
    bool requestAddressBook(const std::string & rpcuser,
                            const std::string & rpcpasswd,
                            const std::string & rpcip,
                            const std::string & rpcport,
                            std::vector<AddressBookEntry> & entries);

    struct Unspent
    {
        std::string txId;
        int vout;
        double amount;
    };
    bool listUnspent(const std::string & rpcuser,
                     const std::string & rpcpasswd,
                     const std::string & rpcip,
                     const std::string & rpcport,
                     std::vector<Unspent> & entries);

    bool decodeRawTransaction(const std::string & rpcuser,
                              const std::string & rpcpasswd,
                              const std::string & rpcip,
                              const std::string & rpcport,
                              const std::string & rawtx,
                              std::string & tx);

    bool signRawTransaction(const std::string & rpcuser,
                            const std::string & rpcpasswd,
                            const std::string & rpcip,
                            const std::string & rpcport,
                            std::string & rawtx);

    bool sendRawTransaction(const std::string & rpcuser,
                            const std::string & rpcpasswd,
                            const std::string & rpcip,
                            const std::string & rpcport,
                            const std::string & rawtx);

    bool getNewAddress(const std::string & rpcuser,
                       const std::string & rpcpasswd,
                       const std::string & rpcip,
                       const std::string & rpcport,
                       std::string & addr);

    bool getTransaction(const std::string & rpcuser,
                        const std::string & rpcpasswd,
                        const std::string & rpcip,
                        const std::string & rpcport,
                        const std::string & txid);
                        // std::string & tx);

    bool getNewPubKey(const std::string & rpcuser,
                      const std::string & rpcpasswd,
                      const std::string & rpcip,
                      const std::string & rpcport,
                      std::string & key);

    bool dumpPrivKey(const std::string & rpcuser,
                     const std::string & rpcpasswd,
                     const std::string & rpcip,
                     const std::string & rpcport,
                     const std::string & address,
                     std::string & key);

    bool importPrivKey(const std::string & rpcuser,
                       const std::string & rpcpasswd,
                       const std::string & rpcip,
                       const std::string & rpcport,
                       const std::string & key,
                       const std::string & label);

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

#endif
