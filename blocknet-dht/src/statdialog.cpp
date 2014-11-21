//*****************************************************************************
//*****************************************************************************

#include "statdialog.h"
#include "blocknetapp.h"

#include <QTextEdit>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>

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
    QHBoxLayout * hbox = new QHBoxLayout;
    QVBoxLayout * vbox = new QVBoxLayout;

    QPushButton * dump = new QPushButton("dump", this);
    hbox->addWidget(dump);

    QPushButton * search = new QPushButton("search", this);
    hbox->addWidget(search);

    hbox->addStretch();

    vbox->addLayout(hbox);

    m_console = new QTextEdit(this);
    vbox->addWidget(m_console);

    setLayout(vbox);

    BlocknetApp * app = qobject_cast<BlocknetApp *>(qApp);
    if (!app)
    {
        return;
    }

    connect(dump, SIGNAL(clicked()),   app, SLOT(onDump()));
    connect(search, SIGNAL(clicked()), app, SLOT(onSearch()));
}
