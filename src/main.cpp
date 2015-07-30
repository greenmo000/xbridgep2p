//*****************************************************************************
//*****************************************************************************

#include "xbridgeapp.h"
#include "xbridgeexchange.h"
#include "util/util.h"
#include "util/settings.h"
#include "version.h"
#include "ExceptionHandler.h"

//*****************************************************************************
//*****************************************************************************
int main(int argc, char *argv[])
{
    std::wstring name(L"xbridgep2p.exe");
    std::wstring mailto(L"xbridge_bugs@aktivsystems.ru");

    Settings & s = settings();

    s.read((std::string(*argv) + ".conf").c_str());
    s.parseCmdLine(argc, argv);

    XBridgeApp & a = XBridgeApp::instance();

#ifdef WIN32
    {
        if (s.logPath().length() > 0)
        {
            ExceptionHandler & e = ExceptionHandler::instance();
            e.init(util::wide_string(s.logPath()),
                   name,
                   util::wide_string(a.version()),
                   mailto);
        }
    }
#endif

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
