//*****************************************************************************
//*****************************************************************************

#ifndef STATDIALOG_H
#define STATDIALOG_H

#include <QDialog>

class QTextEdit;

//*****************************************************************************
//*****************************************************************************
class StatDialog : public QDialog
{
    Q_OBJECT

public:
    StatDialog(QWidget *parent = 0);
    ~StatDialog();

public slots:
    void onLogMessage(const QString & msg);

private:
    void setupUi();

private:
    QTextEdit * m_console;
};

#endif // STATDIALOG_H
