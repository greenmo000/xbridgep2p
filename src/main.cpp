//*****************************************************************************
//*****************************************************************************

#include "xbridgeapp.h"
#include "xbridgeexchange.h"
#include "util/settings.h"
#include "version.h"

//*****************************************************************************
//*****************************************************************************
int main(int argc, char *argv[])
{
    Settings & s = settings();
    s.read((std::string(*argv) + ".conf").c_str());
    s.parseCmdLine(argc, argv);

    XBridgeApp & a = XBridgeApp::instance();

    // init xbridge network
    a.initDht();

    // init exchange
    XBridgeExchange & e = XBridgeExchange::instance();
    e.init();

    int retcode = a.exec();

    // stop
    a.stopDht();

    return retcode;
}
