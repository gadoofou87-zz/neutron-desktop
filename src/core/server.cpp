#include "server.h"
#include "client.h"
#include "file.h"

#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

#include <cryptopp/filters.h>
#include <cryptopp/sha3.h>
using CryptoPP::ArraySink;
using CryptoPP::ArraySource;
using CryptoPP::HashFilter;
using CryptoPP::SHA3_512;

#ifdef __cplusplus
extern "C" {
#include <oqs/oqs.h>
}
#endif

Server::Server()
    : interruptionRequested(false)
    , reading(false)
    , writing(false)
    , encryption(false)
    , shared_secret(OQS_KEM_sike_p751_length_shared_secret)
{
}

Server::~Server()
{
    socket->disconnect(this);
    socket->deleteLater();
    db.close();
}

void Server::run(QTcpSocket *socket, QString id, QString username, QString password, bool signup)
{
    this->socket = socket;
    stream.setDevice(socket);

    this->username = username;
    this->password = password;
    this->signup = signup;

    this->id = QByteArray::fromHex(id.toLatin1());

    db = QSqlDatabase::cloneDatabase(QLatin1String(QSqlDatabase::defaultConnection), {});
    db.open();

    connect(socket, &QTcpSocket::disconnected,
            this, &Server::onDisconnected);
    connect(socket, &QTcpSocket::readyRead,
            this, &Server::onReadyRead);

    disconnectTimer = new QTimer(this);
    disconnectTimer->callOnTimeout([ = ]
    {
        close("Connection timed out");
    });
    disconnectTimer->setInterval(35000);
    disconnectTimer->start();
}

void Server::close(QString reason)
{
    if (!reason.isEmpty())
    {
        emit print(reason);
    }

    socket->close();
}

void Server::onDisconnected()
{
    interruptionRequested = true;

    if (reading)
    {
        connect(this, &Server::read,
                this, &Server::deleteLater);
    }
    else if (writing)
    {
        connect(this, &Server::written,
                this, &Server::deleteLater);
    }
    else
    {
        deleteLater();
    }
}

void Server::onReadyRead()
{
    if (interruptionRequested)
    {
        return;
    }

    reading = true;

    while (!socket->atEnd())
    {
        stream.startTransaction();

        quint8 type;
        quint16 length;
        stream >> type >> length;

        QByteArray in;
        QVector<quint8> crypto[2];

        if (length > 0)
        {
            in.resize(length);

            if (encryption)
            {
                crypto[0].resize(dec.DigestSize());
                crypto[1].resize(dec.DefaultIVLength());

                stream.readRawData(reinterpret_cast<char *>(crypto[0].data()), crypto[0].size());
                stream.readRawData(reinterpret_cast<char *>(crypto[1].data()), crypto[1].size());
            }

            stream.readRawData(in.data(), in.size());
        }

        if (!stream.commitTransaction())
        {
            break;
        }

        QVariant v;

        if (length > 0)
        {
            if (encryption)
            {
                dec.SetKeyWithIV(shared_secret.constData(),
                                 shared_secret.size(),
                                 crypto[1].constData(),
                                 crypto[1].size());

                if (!dec.DecryptAndVerify(reinterpret_cast<quint8 *>(in.data()),
                                          crypto[0].constData(),
                                          crypto[0].size(),
                                          crypto[1].constData(),
                                          crypto[1].size(),
                                          nullptr,
                                          0,
                                          reinterpret_cast<const quint8 *>(in.constData()), in.size()))
                {
                    close(tr("Failed to decrypt incoming packet"));
                    break;
                }
            }

            QDataStream ds(&in, QIODevice::ReadOnly);
            v.load(ds);
        }

        bool ok = true;

        if (!encryption)
        {
            switch (type)
            {
            case quint8(PacketType::Handshake):
            {
                if ((ok = v.canConvert<ServerKeyExchange>()))
                {
                    doHandshake(v.value<ServerKeyExchange>());
                }
            }
            break;
            }
        }
        else
        {
            switch (type)
            {
            case quint8(PacketType::ReAuthorization):
            {
                if ((ok = v.canConvert<ReAuthorization>()))
                {
                    doReAuthorization(v.value<ReAuthorization>());
                }
            }
            break;

            case quint8(PacketType::Established):
            {
                if ((ok = v.canConvert<Established>()))
                {
                    doEstablished(v.value<Established>());
                }
            }
            break;

            case quint8(PacketType::UserState):
            {
                if ((ok = v.canConvert<UserState>()))
                {
                    doUserState(v.value<UserState>());
                }
            }
            break;

            case quint8(PacketType::Message):
            {
                if ((ok = v.canConvert<Message>()))
                {
                    doMessage(v.value<Message>());
                }
            }
            break;

            case quint8(PacketType::ReRoom):
            {
                if ((ok = v.canConvert<ReRoom>()))
                {
                    doReRoom(v.value<ReRoom>());
                }
            }
            break;

            case quint8(PacketType::ReUpload):
            {
                if ((ok = v.canConvert<ReUpload>()))
                {
                    doReUpload(v.value<ReUpload>());
                }
            }
            break;

            case quint8(PacketType::Upload):
            {
                if ((ok = v.canConvert<Upload>()))
                {
                    doUpload(v.value<Upload>());
                }
            }
            break;

            case quint8(PacketType::UploadState):
            {
                if ((ok = v.canConvert<UploadState>()))
                {
                    doUploadState(v.value<UploadState>());
                }
            }
            break;

            case quint8(PacketType::Ping):
            {
                if ((ok = v.canConvert<Ping>()))
                {
                    doPing(v.value<Ping>());
                }
            }
            break;
            }

            if (!ok)
            {
                close("Failed to deserialize incoming packet");
                break;
            }
        }
    }

    reading = false;
    emit read();
}

void Server::doHandshake(ServerKeyExchange d)
{
    QByteArray id;

    SHA3_512 hash;
    id.resize(hash.DigestSize());

    ArraySource(d.public_key->constData(),
                d.public_key->size(), true,
                new HashFilter(
                    hash,
                    new ArraySink(reinterpret_cast<quint8 *>(id.data()), id.size())
                ));

    if (id != this->id)
    {
        close(tr("Server signature public key verification failed"));
        return;
    }


    OQS_STATUS rc;

    rc = OQS_SIG_picnic2_L5_FS_verify(d.public_key[1].constData(),
                                      d.public_key[1].size(),
                                      d.signature.constData(),
                                      d.signature.size(),
                                      d.public_key[0].constData());

    if (rc != OQS_SUCCESS)
    {
        close(tr("Server ephemeral public key verification failed"));
        return;
    }

    QVector<quint8> ciphertext(OQS_KEM_sike_p751_length_ciphertext);

    rc = OQS_KEM_sike_p751_encaps(ciphertext.data(),
                                  shared_secret.data(),
                                  d.public_key[1].constData());

    if (rc != OQS_SUCCESS)
    {
        close(tr("Failed to reach shared secret"));
        return;
    }

    sendOne(PacketType::Handshake, QVariant::fromValue(
                ClientKeyExchange
    {
        ciphertext
    }));

    encryption = true;

    sendOne(PacketType::RtAuthorization, QVariant::fromValue(
                RtAuthorization
    {
        username.toUtf8(),
        password.toUtf8(),
        signup
        ? RtAuthorization::Signup
        : RtAuthorization::Signin
    }));
}

void Server::doReAuthorization(ReAuthorization d)
{
    switch (d.error)
    {
    case ReAuthorization::InvalidUsername:
    {
        close(tr("No user found with this username"));
    }
    break;

    case ReAuthorization::InvalidPassword:
    {
        close(tr("You entered the wrong password"));
    }
    break;

    case ReAuthorization::UserExists:
    {
        close(tr("Username already taken"));
    }
    break;

    default:
        ;
    }
}

void Server::doEstablished(Established d)
{
    emit print(tr("Connected"));

    QSqlQuery query(db);
    query.prepare("INSERT OR REPLACE INTO SERVERS (ID, NAME)"
                  " VALUES (?, ?)");
    query.addBindValue(id);
    query.addBindValue(d.name);

    if (!query.exec())
    {
        Client::error(query.lastError().text());
    }

    emit setName(d.name);

    if (!d.motd.isEmpty())
    {
        emit print(tr("Welcome message: %1").arg(d.motd));
    }

    for (const auto &room : d.rooms)
    {
        query.prepare("INSERT OR REPLACE INTO ROOMS (ID, ID_SERVER, NAME)"
                      " VALUES (?, ?, ?)");
        query.addBindValue(room.id);
        query.addBindValue(id);
        query.addBindValue(room.name);

        if (!query.exec())
        {
            Client::error(query.lastError().text());
        }

        emit insertRoom(room.id,
                        room.name);
    }
}

void Server::doUserState(UserState d)
{
    if (id_room.isEmpty())
    {
        close(tr("Server sent invalid data"));
        return;
    }

    switch (d.state)
    {
    case UserState::Joined:
    {
        emit participantJoined(d.id);
    }
    break;

    case UserState::Left:
    {
        emit participantLeft(d.id);
    }
    break;
    }
}

void Server::doMessage(Message d)
{
    if (id_room.isEmpty())
    {
        close(tr("Server sent invalid data"));
        return;
    }

    QSqlQuery query(db);
    query.prepare("INSERT INTO ARCHIVE (TIMESTAMP, ID_SERVER, ID_MESSAGE, ID_ROOM, ID_SENDER, CONTENT)"
                  " VALUES (?, ?, ?, ?, ?, ?)");
    query.addBindValue(d.timestamp);
    query.addBindValue(id);
    query.addBindValue(d.id);
    query.addBindValue(id_room);
    query.addBindValue(d.id_sender);
    query.addBindValue(d.content);

    if (!query.exec())
    {
        Client::error(query.lastError().text());
    }

    emit messageReceived(QDateTime::fromSecsSinceEpoch(d.timestamp),
                         d.id_sender,
                         d.content);
}

void Server::doReRoom(ReRoom d)
{
    switch (d.response)
    {
    case ReRoom::Joined:
    {
        emit joinedRoom();

        QSqlQuery query(db);
        query.prepare("SELECT ID_MESSAGE"
                      " FROM ARCHIVE"
                      " WHERE ID_SERVER = ?"
                      " AND ID_ROOM = ?"
                      " ORDER BY ID DESC"
                      " LIMIT 1");
        query.addBindValue(id);
        query.addBindValue(id_room);

        if (!query.exec())
        {
            Client::error(query.lastError().text());
        }

        if (!query.next())
        {
            return;
        }

        sendOne(PacketType::Synchronize, QVariant::fromValue(
                    Synchronize
        {
            query.value(0).toByteArray()
        }));
    }
    break;

    case ReRoom::Left:
    {
        emit leftRoom();
    }
    break;
    }
}

void Server::doReUpload(ReUpload d)
{
    if (!usershare.contains(d.id))
    {
        close(tr("Server sent invalid data"));
        return;
    }

    switch (d.response)
    {
    case ReUpload::ErrorOccurred:
    {
        usershare.remove(d.id);

        switch (d.error)
        {
        case ReUpload::InternalServerError:
        {
            emit print(tr("Server reported an internal error"));
        }
        break;

        case ReUpload::BadRequest:
        {
            emit print(tr("The requested file has a different size on the server"));
        }
        break;

        case ReUpload::NotFound:
        {
            emit print(tr("The requested file was not found on the server"));
        }
        break;

        default:
            ;
        }
    }
    break;

    case ReUpload::ReadyRead:
    {
        doUploadState(
        {
            d.id,
            UploadState::Next
        });
    }
    break;

    case ReUpload::ReadyWrite:
    {
        sendOne(PacketType::UploadState, QVariant::fromValue(
                    UploadState
        {
            d.id,
            UploadState::Next
        }));
    }
    break;
    }
}

void Server::doUpload(Upload d)
{
    if (!usershare.contains(d.id))
    {
        close(tr("Server sent invalid data"));
        return;
    }

    auto file = usershare.value(d.id);

    if (file->isCancellationRequested())
    {
        usershare.remove(d.id);
        return;
    }

    if (d.chunkdata.isEmpty())
    {
        close(tr("Server sent an empty data chunk"));
        return;
    }

    if (d.chunkdata.size() > file->getRemained())
    {
        close(tr("Server sent more data than required"));
        return;
    }

    file->write(d.chunkdata);

    emit chunkTransferred(d.id, file->getLastWritten());

    if (file->atEnd())
    {
        usershare.remove(d.id);

        sendOne(PacketType::UploadState, QVariant::fromValue(
                    UploadState
        {
            d.id,
            UploadState::Completed
        }));
    }
    else
    {
        sendOne(PacketType::UploadState, QVariant::fromValue(
                    UploadState
        {
            d.id,
            UploadState::Next
        }));
    }
}

void Server::doUploadState(UploadState d)
{
    if (!usershare.contains(d.id))
    {
        close(tr("Server sent invalid data"));
        return;
    }

    auto file = usershare.value(d.id);

    emit chunkTransferred(d.id, file->getLastRead());

    switch (d.state)
    {
    case UploadState::Next:
    {
        if (file->isCancellationRequested())
        {
            usershare.remove(d.id);
            return;
        }

        if (file->atEnd())
        {
            close(tr("Server requested more data than required"));
            return;
        }

        sendOne(PacketType::Upload, QVariant::fromValue(
                    Upload
        {
            d.id,
            file->read()
        }));
    }
    break;

    case UploadState::Completed:
    {
        usershare.remove(d.id);
    }
    break;

    default:
        ;
    }
}

void Server::doPing(Ping d)
{
    disconnectTimer->start();

    sendOne(PacketType::Pong, QVariant::fromValue(d));
}

void Server::sendOne(PacketType type, const QVariant &v)
{
    if (interruptionRequested)
    {
        return;
    }

    writing = true;

    QByteArray out;
    QVector<quint8> crypto[2];

    if (!v.isNull())
    {
        QDataStream ds(&out, QIODevice::WriteOnly);
        v.save(ds);

        if (encryption)
        {
            crypto[0].resize(enc.DigestSize());
            crypto[1].resize(enc.DefaultIVLength());

            enc.GetNextIV(rng, crypto[1].data());
            enc.SetKeyWithIV(shared_secret.constData(),
                             shared_secret.size(),
                             crypto[1].constData(),
                             crypto[1].size());
            enc.EncryptAndAuthenticate(reinterpret_cast<quint8 *>(out.data()),
                                       crypto[0].data(),
                                       crypto[0].size(),
                                       crypto[1].constData(),
                                       crypto[1].size(),
                                       nullptr,
                                       0,
                                       reinterpret_cast<const quint8 *>(out.constData()), out.size());
        }
    }

    QByteArray t;
    QDataStream ds(&t, QIODevice::WriteOnly);

    ds << quint8(type) << quint16(out.size());

    if (!v.isNull())
    {
        if (encryption)
        {
            ds.writeRawData(reinterpret_cast<const char *>(crypto[0].constData()), crypto[0].size());
            ds.writeRawData(reinterpret_cast<const char *>(crypto[1].constData()), crypto[1].size());
        }

        ds.writeRawData(out.constData(), out.size());
    }

    socket->write(t.constData(), t.size());
    socket->flush();

    writing = false;
    emit written();
}

const QByteArray &Server::getId() const
{
    return id;
}

const QString &Server::getUsername() const
{
    return username;
}

bool Server::isTransferring() const
{
    return !usershare.empty();
}

bool Server::isTransferExists(const QByteArray &id) const
{
    return usershare.contains(id);
}

void Server::cancelTransfer(QByteArray id)
{
    if (!usershare.contains(id))
    {
        return;
    }

    usershare.value(id)->requestCancellation();

    sendOne(PacketType::UploadState, QVariant::fromValue(
                UploadState
    {
        id,
        UploadState::Canceled
    }));
}

void Server::joinRoom(QByteArray id)
{
    id_room = id;

    sendOne(PacketType::RtRoom, QVariant::fromValue(
                RtRoom
    {
        id,
        RtRoom::Join
    }));
}

void Server::leaveRoom()
{
    id_room.clear();

    sendOne(PacketType::RtRoom, QVariant::fromValue(
                RtRoom
    {
        {},
        RtRoom::Leave
    }));
}

void Server::receiveFile(QSharedPointer<File> file, QByteArray id)
{
    file->setId(id);

    usershare.insert(id, file);

    sendOne(PacketType::RtUpload, QVariant::fromValue(
                RtUpload
    {
        id,
        file->size(),
        RtUpload::Receive
    }));
}

void Server::sendFile(QSharedPointer<File> file)
{
    auto id = QUuid::createUuid().toRfc4122();

    file->setId(id);

    usershare.insert(id, file);

    sendOne(PacketType::RtUpload, QVariant::fromValue(
                RtUpload
    {
        id,
        file->size(),
        RtUpload::Transmit
    }));
}

void Server::sendMessage(qint64 timestamp, QString content)
{
    auto id = QUuid::createUuid().toRfc4122();

    QSqlQuery query(db);
    query.prepare("INSERT INTO ARCHIVE (TIMESTAMP, ID_SERVER, ID_MESSAGE, ID_ROOM, ID_SENDER, CONTENT)"
                  " VALUES (?, ?, ?, ?, ?, ?)");
    query.addBindValue(timestamp);
    query.addBindValue(this->id);
    query.addBindValue(id);
    query.addBindValue(id_room);
    query.addBindValue(username);
    query.addBindValue(content);

    if (!query.exec())
    {
        Client::error(query.lastError().text());
    }

    sendOne(PacketType::Message, QVariant::fromValue(
                Message
    {
        {},
        id,
        {},
        content
    }));
}
