//******************************************************************************
//******************************************************************************

#include "logger.h"
#include "settings.h"
#include "../uiconnector.h"

#include <string>
#include <sstream>
#include <fstream>
#include <thread>
#include <mutex>
#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

std::mutex logLocker;

//******************************************************************************
//******************************************************************************
// static
std::string LOG::m_logFileName;

//******************************************************************************
//******************************************************************************
LOG::LOG(const char reason)
    : std::basic_stringstream<char, std::char_traits<char>,
                    boost::pool_allocator<char> >()
    , m_r(reason)
{
    *this << "\n" << "[" << (char)std::toupper(m_r) << "] "
          << boost::posix_time::second_clock::local_time()
          << " [0x" << std::this_thread::get_id() << "] ";
}

//******************************************************************************
//******************************************************************************
// static
std::string LOG::logFileName()
{
    return m_logFileName;
}

//******************************************************************************
//******************************************************************************
LOG::~LOG()
{
    std::lock_guard<std::mutex> lock(logLocker);

    // const static std::string path     = settings().logPath().size() ? settings().logPath() : settings().appPath();
    const static bool logToFile       = true; // !path.empty();
    static boost::gregorian::date day =
            boost::gregorian::day_clock::local_day();
    if (m_logFileName.empty())
    {
        m_logFileName    = makeFileName();
    }

    std::cout << str().c_str();

    try
    {
        std::string copy(str().c_str());
        uiConnector.NotifyLogMessage(copy);

        if (logToFile)
        {
            boost::gregorian::date tmpday =
                    boost::gregorian::day_clock::local_day();

            if (day != tmpday)
            {
                m_logFileName = makeFileName();
                day = tmpday;
            }

            std::ofstream file(m_logFileName.c_str(), std::ios_base::app);
            file << str().c_str();
        }
    }
    catch (std::exception &)
    {
    }
}

//******************************************************************************
//******************************************************************************
// static
std::string LOG::makeFileName()
{
    std::string path = settings().logPath().size() ?
                       settings().logPath() :
                       settings().appPath();

    if (!path.size())
    {
        assert(!"wtf? empty path???");
        return std::string();
    }

#ifdef WIN32
    char sep = '\\';
    std::replace(path.begin(), path.end(), '/', sep);
#else
    char sep = '/';
    std::replace(path.begin(), path.end(), '\\', sep);
#endif

    if (path[path.size()-1] != sep)
    {
        path.append(1, sep);
    }

    return path +
            "xbridgep2p_" +
            boost::posix_time::to_iso_string(boost::posix_time::second_clock::local_time()) +
            ".log";
}
