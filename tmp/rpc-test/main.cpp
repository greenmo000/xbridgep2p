#include <QCoreApplication>

#include "bitcoinrpc.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    rpc::sendRawTransaction();

    return a.exec();
}
