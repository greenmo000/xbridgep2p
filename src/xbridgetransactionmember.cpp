//*****************************************************************************
//*****************************************************************************

#include "xbridgetransactionmember.h"

//*****************************************************************************
//*****************************************************************************
bool XBridgeTransactionMember::operator == (const std::vector<unsigned char> & addr)
{
    if (m_sourceAddr.size() != addr.size())
    {
        return false;
    }
    return m_sourceAddr == addr;
}

