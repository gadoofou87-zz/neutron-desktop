#include "mainwindow.h"
#include "core/client.h"
#include "core/file.h"
#include "core/packet.h"

#include <QApplication>
#include <QTextCursor>

int main(int argc, char *argv[])
{
    qRegisterMetaType<QAbstractSocket::SocketError>("SocketError");
    qRegisterMetaType<QSharedPointer<File>>("QSharedPointer<File>");
    qRegisterMetaType<QTextCursor>("QTextCursor");
    qRegisterMetaTypeStreamOperators<ServerKeyExchange>("ServerKeyExchange");
    qRegisterMetaTypeStreamOperators<ClientKeyExchange>("ClientKeyExchange");
    qRegisterMetaTypeStreamOperators<RtAuthorization>("RtAuthorization");
    qRegisterMetaTypeStreamOperators<ReAuthorization>("ReAuthorization");
    qRegisterMetaTypeStreamOperators<Room>("Room");
    qRegisterMetaTypeStreamOperators<Established>("Established");
    qRegisterMetaTypeStreamOperators<Synchronize>("Synchronize");
    qRegisterMetaTypeStreamOperators<UserState>("UserState");
    qRegisterMetaTypeStreamOperators<Message>("Message");
    qRegisterMetaTypeStreamOperators<RtRoom>("RtRoom");
    qRegisterMetaTypeStreamOperators<ReRoom>("ReRoom");
    qRegisterMetaTypeStreamOperators<RtUpload>("RtUpload");
    qRegisterMetaTypeStreamOperators<ReUpload>("ReUpload");
    qRegisterMetaTypeStreamOperators<Upload>("Upload");
    qRegisterMetaTypeStreamOperators<UploadState>("UploadState");
    qRegisterMetaTypeStreamOperators<Ping>("Ping");

    QApplication a(argc, argv);
    a.setApplicationName("Neutron");
    Client();
    MainWindow w;
    return QApplication::exec();
}
