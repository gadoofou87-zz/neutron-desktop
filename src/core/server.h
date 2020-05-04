#ifndef SERVER_H
#define SERVER_H

#include "packet.h"

#include <QDataStream>
#include <QHash>
#include <QSqlDatabase>
#include <QTcpSocket>
#include <QTimer>

#include <cryptopp/chachapoly.h>
#include <cryptopp/osrng.h>

class File;
class Server : public QObject
{
    Q_OBJECT
public:
    explicit Server();
    ~Server();

    const QByteArray &getId() const;
    const QString &getUsername() const;

    bool isTransferring() const;
    bool isTransferExists(const QByteArray &) const;

signals:
    void chunkTransferred(QByteArray, qint64);
    void insertRoom(QByteArray, QString);
    void joinedRoom();
    void leftRoom();
    void messageReceived(QDateTime, QString, QString);
    void participantJoined(QString);
    void participantLeft(QString);
    void print(QString);
    void setName(QString);

public slots:
    void run(QTcpSocket *, QString, QString, QString, bool);
    void close(QString = {});

    void cancelTransfer(QByteArray);
    void joinRoom(QByteArray);
    void leaveRoom();
    void receiveFile(QSharedPointer<File>, QByteArray);
    void sendFile(QSharedPointer<File>);
    void sendMessage(qint64, QString);

signals:
    void read();
    void written();

private slots:
    void onDisconnected();
    void onReadyRead();

private:
    QTcpSocket *socket;
    QDataStream stream;

    QString username;
    QString password;
    bool signup;

    QByteArray id;
    QByteArray id_room;

    bool interruptionRequested;
    bool reading;
    bool writing;

    bool encryption;
    QVector<quint8> shared_secret;

    CryptoPP::AutoSeededRandomPool rng;
    CryptoPP::XChaCha20Poly1305::Decryption dec;
    CryptoPP::XChaCha20Poly1305::Encryption enc;

    QHash<QByteArray, QSharedPointer<File>> usershare;
    QSqlDatabase db;

    QTimer *disconnectTimer;

    void doHandshake(ServerKeyExchange);
    void doReAuthorization(ReAuthorization);
    void doEstablished(Established);
    void doUserState(UserState);
    void doMessage(Message);
    void doReRoom(ReRoom);
    void doReUpload(ReUpload);
    void doUpload(Upload);
    void doUploadState(UploadState);
    void doPing(Ping);

    void sendOne(PacketType, const QVariant & = {});
};

#endif // SERVER_H
