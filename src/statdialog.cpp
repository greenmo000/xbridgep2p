//*****************************************************************************
//*****************************************************************************

#include "statdialog.h"
#include "xbridgeapp.h"

#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QScreen>

//*****************************************************************************
//*****************************************************************************
StatDialog::StatDialog(QWidget *parent)
    : QDialog(parent)
    , m_console(0)
{
    setupUi();
}

//*****************************************************************************
//*****************************************************************************
StatDialog::~StatDialog()
{

}

//*****************************************************************************
//*****************************************************************************
void StatDialog::onLogMessage(const QString & msg)
{
    if (m_console)
    {
        m_console->append(msg);
    }
}

//*****************************************************************************
//*****************************************************************************
void StatDialog::setupUi()
{
    QRect r = qApp->primaryScreen()->geometry();
    setGeometry(r.width()/2-400, r.height()/2-200, 800, 400);

    QHBoxLayout * hbox = new QHBoxLayout;
    QVBoxLayout * vbox = new QVBoxLayout;

//    QPushButton * generate = new QPushButton("generate", this);
//    hbox->addWidget(generate);

    QPushButton * dump = new QPushButton("dump", this);
    hbox->addWidget(dump);

    hbox->addStretch();
    vbox->addLayout(hbox);

    hbox = new QHBoxLayout;

    m_searchText = new QLineEdit(this);
    hbox->addWidget(m_searchText);

    QPushButton * search = new QPushButton("search", this);
    hbox->addWidget(search);

    QPushButton * send = new QPushButton("send", this);
    hbox->addWidget(send);

    vbox->addLayout(hbox);

    m_console = new QTextEdit(this);
    vbox->addWidget(m_console);

    setLayout(vbox);

    XBridgeApp * app = qobject_cast<XBridgeApp *>(qApp);
    if (!app)
    {
        return;
    }

//    connect(generate, SIGNAL(clicked()), app,  SLOT(onGenerate()));
    connect(dump,     SIGNAL(clicked()), app,  SLOT(onDump()));
    connect(search,   SIGNAL(clicked()), this, SLOT(onSearch()));
    connect(send,     SIGNAL(clicked()), this, SLOT(onSend()));
}

//*****************************************************************************
//*****************************************************************************
void StatDialog::onSearch()
{
    XBridgeApp * app = qobject_cast<XBridgeApp *>(qApp);
    if (!app)
    {
        return;
    }

    app->onSearch(m_searchText->text().toStdString());
}

//*****************************************************************************
//*****************************************************************************
void StatDialog::onSend()
{
    XBridgeApp * app = qobject_cast<XBridgeApp *>(qApp);
    if (!app)
    {
        return;
    }

    std::string saddr = m_searchText->text().toStdString();
    std::string sdata = "test message";

    std::vector<unsigned char> addr;
    std::copy(saddr.begin(), saddr.end(), std::back_inserter(addr));

    std::vector<unsigned char> data;
    std::copy(sdata.begin(), sdata.end(), std::back_inserter(data));

    app->onSend(addr, data);
}
