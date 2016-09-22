//*****************************************************************************
//*****************************************************************************

#ifndef XBRIDGESESSIONBTC_H
#define XBRIDGESESSIONBTC_H

#include "xbridge.h"
#include "xbridgesession.h"
#include "xbridgepacket.h"
#include "xbridgetransaction.h"
#include "xbridgewallet.h"
#include "FastDelegate.h"
#include "util/uint256.h"

#include <memory>
#include <set>
#include <boost/thread/mutex.hpp>
#include <boost/noncopyable.hpp>

//*****************************************************************************
//*****************************************************************************
class XBridgeSessionBtc
        : public XBridgeSession
{
public:
    XBridgeSessionBtc();
    XBridgeSessionBtc(const WalletParam & wallet);
    virtual ~XBridgeSessionBtc();

public:
    std::shared_ptr<XBridgeSessionBtc> shared_from_this()
    {
        return std::static_pointer_cast<XBridgeSessionBtc>(XBridgeSession::shared_from_this());
    }

protected:
    // virtual uint32_t lockTime(const char role) const;
    virtual std::string createRawTransaction(const std::vector<std::pair<std::string, int> > & inputs,
                                             const CPubKey & my1,
                                             const CPubKey & r1,
                                             const CPubKey & x,
                                             const std::vector<std::pair<std::string, double> > & outputs,
                                             const uint32_t lockTime);
};

typedef std::shared_ptr<XBridgeSessionBtc> XBridgeSessionBtcPtr;

#endif // XBRIDGESESSIONBTC_H
