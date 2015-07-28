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
    std::wstring name(L"abrisgeptp.exe");
    std::wstring version(L"v.0.4");
    std::wstring mailto(L"xbridge_bugs@aktivsystems.ru");

    Settings & s = settings();

#ifdef WIN32
    {
        if (s.logPath().length() > 0)
        {
            ExceptionHandler::instance().init(util::wide_string(s.logPath()), name, version, mailto);
        }
    }
#endif

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
