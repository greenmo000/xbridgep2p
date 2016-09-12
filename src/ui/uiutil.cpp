//******************************************************************************
//******************************************************************************

#include "uiutil.h"
#include "util/util.h"
#include "util/settings.h"

#include <boost/algorithm/string.hpp>

#include <QWidget>
#include <QApplication>
#include <QDesktopWidget>


//******************************************************************************
//******************************************************************************
namespace gui
{

//******************************************************************************
//******************************************************************************
void saveWindowPos(const char * name, const QWidget & window)
{
    std::string keyname = std::string("Gui.") + name;

    QRect g = window.geometry();

    Settings & s = settings();
    if (window.isMaximized())
    {
        std::string size = util::base64_decode(s.get<std::string>(keyname.c_str()));
        if (size.size())
        {
            size.pop_back();
            size += '1';

            s.set<std::string>(keyname.c_str(), util::base64_encode(size));
        }
    }
    else
    {
        std::ostringstream o;
        o << g.x() << ',' << g.y() << ','
          << g.width() << ',' << g.height() << ",0";

        s.set<std::string>(keyname.c_str(), util::base64_encode(o.str()));
    }
}

//******************************************************************************
//******************************************************************************
void restoreWindowPos(const char * name, QWidget & window)
{
    Settings & s = settings();
    std::string size = s.get<std::string>((std::string("Gui.") + name).c_str());
    std::vector<std::string> vals;
    boost::split(vals, util::base64_decode(size), boost::is_any_of(","));
    if (vals.size() >= 4)
    {
        QRect g(atoi(vals[0].c_str()), atoi(vals[1].c_str()),
                atoi(vals[2].c_str()), atoi(vals[3].c_str()));

        // TODO need to fix coordinates
        // QRect r = qApp->desktop()->availableGeometry(&window);
        // QRect i = g.intersected(r);

        window.setGeometry(g);
    }
    if (vals.size() >= 5)
    {
        bool isMax = atoi(vals[4].c_str()) > 0;
        if (isMax)
        {
            window.showMaximized();
        }
    }
}

} // namespace gui
