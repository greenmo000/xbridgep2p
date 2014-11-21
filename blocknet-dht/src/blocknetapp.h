//*****************************************************************************
//*****************************************************************************
#ifndef BLOCKNETAPP_H
#define BLOCKNETAPP_H

#include <QApplication>

#include <thread>
#include <atomic>
#include <vector>

#include <Ws2tcpip.h>

//*****************************************************************************
//*****************************************************************************
class BlocknetApp : public QApplication
{
    Q_OBJECT

public:
    BlocknetApp(int argc, char *argv[]);
    virtual ~BlocknetApp();

signals:
    void showLogMessage(const QString & msg);

public:
    bool initDht();
    bool stopDht();

    void logMessage(const QString & msg);

public slots:
    void onDump();
    void onSearch();

public:
    static void sleep(const unsigned int umilliseconds);

private:
    void dhtThreadProc();

private:
    std::thread       m_dhtThread;
    std::atomic<bool> m_dhtStop;

    std::atomic<bool> m_signalDump;
    std::atomic<bool> m_signalSearch;

    const bool        m_ipv4;
    const bool        m_ipv6;

    unsigned short    m_dhtPort;

    std::vector<sockaddr_storage> m_nodes;
};

#endif // BLOCKNETAPP_H
