#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "connectdialog.h"
#include "historyform.h"
#include "transferdialog.h"
#include "core/client.h"
#include "core/file.h"
#include "core/server.h"

#include <KActionMenu>
#include <KColorSchemeManager>
#include <QClipboard>
#include <QDateTime>
#include <QDesktopServices>
#include <QFileDialog>
#include <QHostAddress>
#include <QImageReader>
#include <QInputDialog>
#include <QKeyEvent>
#include <QMessageBox>
#include <QMimeDatabase>
#include <QMimeData>
#include <QNetworkProxy>
#include <QSharedPointer>
#include <QSound>
#include <QTcpSocket>
#include <QUrlQuery>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    auto schemes = new KColorSchemeManager(this);
    auto menu = schemes->createSchemeSelectionMenu(QStringLiteral(), this);

    ui->setupUi(this);
    ui->actionColor_Theme->setMenu(menu->menu());
    ui->lineEdit->installEventFilter(this);

    installEventFilter(this);
    setAcceptDrops(true);

    connect(ui->chatBrowser, &ChatBrowser::anchorClicked,
            this, &MainWindow::onAnchorClicked);
    connect(ui->treeWidget, &QTreeWidget::itemDoubleClicked,
            this, &MainWindow::onItemDoubleClicked);
    connect(ui->lineEdit, &QLineEdit::returnPressed,
            this, &MainWindow::onReturnPressed);

    connect(ui->actionConnect, &QAction::triggered,
            this, &MainWindow::onConnect);
    connect(ui->actionDisconnect, &QAction::triggered,
            this, &MainWindow::onDisconnect);
    connect(ui->actionLeave_Room, &QAction::triggered,
            this, &MainWindow::onLeaveRoom);

    connect(ui->actionHistory, &QAction::triggered,
            this, &MainWindow::onHistory);

    show();
}

MainWindow::~MainWindow()
{
    delete ui;
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == this)
    {
        if (event->type() == QEvent::DragEnter)
        {
            event->accept();
        }
        else if (event->type() == QEvent::Drop)
        {
            auto mimeData = static_cast<QDropEvent *>(event)->mimeData();

            if (!mimeData->hasUrls())
            {
                return false;
            }

            sendFile(mimeData);
        }
        else
        {
            return false;
        }

        return true;
    }
    else if (watched == ui->lineEdit)
    {
        if (event->type() == QEvent::KeyPress)
        {
            auto keyEvent = static_cast<QKeyEvent *>(event);

            if (keyEvent->matches(QKeySequence::Paste))
            {
                auto mimeData = QGuiApplication::clipboard()->mimeData();

                int ret;

                if (mimeData->hasUrls())
                {
                    ret = QMessageBox::question(this, QString(),
                                                tr("You're trying to paste a file\n") +
                                                tr("Are you sure you want to paste the clipboard file into the chat window?"));
                }
                else if (mimeData->hasImage())
                {
                    ret = QMessageBox::question(this, QString(),
                                                tr("You're trying to insert an image\n") +
                                                tr("Are you sure you want to paste the clipboard image into the chat window?"));
                }
                else
                {
                    return false;
                }

                if (ret != QMessageBox::Yes)
                {
                    return true;
                }

                sendFile(mimeData);
            }
            else
            {
                return false;
            }

            return true;
        }

        return false;
    }

    return false;
}

void MainWindow::onAnchorClicked(const QUrl &url)
{
    if (url.scheme() == "neutron")
    {
        if (!check(false, false))
        {
            return;
        }

        if (url.hasFragment())
        {
            if (server->getId() != QByteArray::fromHex(url.fragment().toLatin1()))
            {
                ui->chatBrowser->append(tr("This download is located on a different server"));
                return;
            }
        }

        if (url.host() == "file")
        {
            QUrlQuery q(url);

            if (!q.hasQueryItem("id")
                    || !q.hasQueryItem("name")
                    || !q.hasQueryItem("size"))
            {
                ui->chatBrowser->append(tr("Not enough arguments"));
                return;
            }

            auto id = QByteArray::fromHex(q.queryItemValue("id").toLatin1());
            auto name = q.queryItemValue("name");
            auto size = q.queryItemValue("size").toLongLong();

            if (size < 1)
            {
                ui->chatBrowser->append(tr("Invalid file size"));
                return;
            }

            if (server->isTransferExists(id))
            {
                ui->chatBrowser->append(tr("This file is already downloading"));
                return;
            }

            auto fileName = QFileDialog::getSaveFileName(this, {}, name);

            if (fileName.isEmpty())
            {
                return;
            }

            QSharedPointer<File> file(new File(fileName));

            if (!file->open(QIODevice::ReadWrite))
            {
                ui->chatBrowser->append(tr("Unable to create file"));
                return;
            }

            if (!file->resize(size))
            {
                ui->chatBrowser->append(tr("Unable to resize file"));
                return;
            }

            auto d = new TransferDialog(this, server, file);

            connect(d, &TransferDialog::completed, this, [ = ]
            {
                onFileReceived(file);
            });

            QMetaObject::invokeMethod(server, "receiveFile",
                                      Q_ARG(QSharedPointer<File>, file),
                                      Q_ARG(QByteArray, id));
        }
        else
        {
            ui->chatBrowser->append(tr("Unrecognized link type"));
        }
    }
    else
    {
        QDesktopServices::openUrl(url);
    }
}

void MainWindow::onItemDoubleClicked(const QTreeWidgetItem *item)
{
    if (!item->parent())
    {
        return;
    }

    if (!check(false, true))
    {
        return;
    }

    auto connection = new QMetaObject::Connection;
    *connection = connect(server, &Server::joinedRoom, this, [ = ]
    {
        roomParticipant = true;

        ui->listWidget->clear();
        ui->listWidget->addItem(server->getUsername());

        ui->chatBrowser->clear();
        ui->chatBrowser->append(tr("You have joined the room"));

        setWindowTitle(QString("%1 - %2")
                       .arg(item->text(0))
                       .arg(qApp->applicationName()));

        disconnect(*connection);
        delete connection;
    }, Qt::QueuedConnection);

    QMetaObject::invokeMethod(server, "joinRoom",
                              Q_ARG(QByteArray, item->data(0, Qt::UserRole).toByteArray()));
}

void MainWindow::onReturnPressed()
{
    if (!check(true, false))
    {
        return;
    }

    auto message = ui->lineEdit->text().trimmed();

    if (message.isEmpty())
    {
        return;
    }

    sendMessage(message);
}

void MainWindow::onConnect()
{
    ConnectDialog d(this);
    connect(&d, &ConnectDialog::dataEntered,
            this, &MainWindow::connectToHost);
    d.exec();
}

void MainWindow::onDisconnect()
{
    if (!check(false, true))
    {
        return;
    }

    QMetaObject::invokeMethod(server, "close");
}

void MainWindow::onLeaveRoom()
{
    if (!check(true, true))
    {
        return;
    }

    QMetaObject::invokeMethod(server, "leaveRoom");
}

void MainWindow::onHistory()
{
    new HistoryForm(this);
}

void MainWindow::onInsertRoom(QByteArray id, QString name)
{
    auto item = new QTreeWidgetItem(root);
    item->setData(0, Qt::UserRole, id);
    item->setText(0, name);
}

void MainWindow::onFileReceived(QSharedPointer<File> file)
{
    QMimeDatabase db;
    auto mime = db.mimeTypeForFile(*file)
                .name()
                .toLatin1();

    if (QImageReader::supportedMimeTypes().contains(mime))
    {
        QImage image;
        QImageReader reader(file.data());

        file->seek(0);

        if (!reader.read(&image))
        {
            ui->chatBrowser->append(tr("Cannot preview image"));
            return;
        }

        if (image.width() > PREVIEW_SIZE || image.height() > PREVIEW_SIZE)
        {
            image = image.width() > image.height()
                    ? image.scaledToWidth(PREVIEW_SIZE, Qt::SmoothTransformation)
                    : image.scaledToHeight(PREVIEW_SIZE, Qt::SmoothTransformation);
        }

        ui->chatBrowser->moveCursor(QTextCursor::End);
        ui->chatBrowser->textCursor().insertBlock();
        ui->chatBrowser->textCursor().insertImage(image);
    }

    ui->chatBrowser->append(tr("Download %1 completed").arg(file->getName()));
}

void MainWindow::onFileSent(QSharedPointer<File> file)
{
    sendMessage(QUrl(QString("neutron://file?"
                             "&id=%1"
                             "&name=%2"
                             "&size=%3")
                     .arg(QString(file->getId().toHex()))
                     .arg(file->getName())
                     .arg(file->size()))
                .toEncoded());
}

void MainWindow::onLeftRoom()
{
    roomParticipant = false;

    ui->listWidget->clear();
    ui->chatBrowser->append(tr("You left the room"));

    setWindowTitle(QString());
}

void MainWindow::onMessageReceived(QDateTime dt, QString sender, QString message)
{
    ui->chatBrowser->append(message, sender, dt);

    if (isActiveWindow())
    {
        return;
    }

    QApplication::alert(this);
    QSound::play(":/data/sounds/intuition.wav");
}

void MainWindow::onParticipantJoined(QString id)
{
    ui->listWidget->addItem(id);
}

void MainWindow::onParticipantLeft(QString id)
{
    auto items = ui->listWidget->findItems(id, Qt::MatchExactly);

    if (items.isEmpty())
    {
        return;
    }

    delete items.at(0);
}

void MainWindow::onPrint(QString s)
{
    ui->chatBrowser->append(s);
}

void MainWindow::onSetName(QString name)
{
    root->setText(0, name);
}

bool MainWindow::check(bool participant, bool transfer)
{
    bool result;

    if ((result = server))
    {
        bool is;

        if (participant)
        {
            result *= (is = roomParticipant);

            if (!is)
            {
                ui->chatBrowser->append(tr("You are not in any room"));
            }
        }

        if (transfer)
        {
            result *= (is = !server->isTransferring());

            if (!is)
            {
                ui->chatBrowser->append(tr("File transfer in progress"));
            }
        }
    }
    else
    {
        ui->chatBrowser->append(tr("You are not connected to the server"));
    }

    return result;
}

void MainWindow::connectToHost(const QString &server_id,
                               const QString &server_host, int server_port,
                               const QString &username, const QString &password, bool signup,
                               const QString &proxy_host, int proxy_port,
                               const QString &proxy_user, const QString &proxy_pass)
{
    if (socket)
    {
        connect(socket, &QObject::destroyed, this, [ = ]
        {
            connectToHost(server_id,
                          server_host, server_port,
                          username, password, signup,
                          proxy_host, proxy_port,
                          proxy_user, proxy_pass);
        }, Qt::QueuedConnection);

        if (server)
        {
            QMetaObject::invokeMethod(server, "close");
        }
        else
        {
            socket->deleteLater();
        }

        return;
    }

    socket = new QTcpSocket;

    connect(socket, &QTcpSocket::connected, this, [ = ]
    {
        root = new QTreeWidgetItem(ui->treeWidget);
        root->setExpanded(true);
        root->setText(0, "Root");

        server = new Server;
        server->moveToThread(Client::getWorkerThread());

        connect(server, &Server::insertRoom,
                this, &MainWindow::onInsertRoom);
        connect(server, &Server::leftRoom,
                this, &MainWindow::onLeftRoom);
        connect(server, &Server::messageReceived,
                this, &MainWindow::onMessageReceived);
        connect(server, &Server::participantJoined,
                this, &MainWindow::onParticipantJoined);
        connect(server, &Server::participantLeft,
                this, &MainWindow::onParticipantLeft);
        connect(server, &Server::print,
                this, &MainWindow::onPrint);
        connect(server, &Server::setName,
                this, &MainWindow::onSetName);

        QMetaObject::invokeMethod(server, "run",
                                  Q_ARG(QTcpSocket *, socket),
                                  Q_ARG(QString, server_id),
                                  Q_ARG(QString, username),
                                  Q_ARG(QString, password),
                                  Q_ARG(bool, signup));
    }, Qt::QueuedConnection);
    connect(socket, &QTcpSocket::disconnected, this, [ = ]
    {
        ui->chatBrowser->append(tr("Server connection closed"));

        ui->listWidget->clear();
        ui->treeWidget->clear();

        roomParticipant = false;
    }, Qt::QueuedConnection);
    connect(socket, QOverload<QTcpSocket::SocketError>::of(&QTcpSocket::error), this, [ = ]
    {
        ui->chatBrowser->append(socket->errorString());

        if (!server)
        {
            if (socket)
            {
                socket->deleteLater();
            }
        }
    }, Qt::BlockingQueuedConnection);

    ui->chatBrowser->clear();
    ui->chatBrowser->append(tr("Connecting to %1:%2")
                            .arg(server_host)
                            .arg(server_port));

    if (!proxy_host.isEmpty())
    {
        QNetworkProxy proxy;
        proxy.setType(QNetworkProxy::Socks5Proxy);
        proxy.setHostName(proxy_host);
        proxy.setPort(proxy_port);
        proxy.setUser(proxy_user);
        proxy.setPassword(proxy_pass);

        socket->setProxy(proxy);
    }

    socket->connectToHost(server_host, server_port);
    socket->moveToThread(Client::getWorkerThread());
}

void MainWindow::sendFile(const QMimeData *mimeData)
{
    if (!check(true, false))
    {
        return;
    }

    if (mimeData->hasUrls())
    {
        for (const auto &url : mimeData->urls())
        {
            sendFile(QSharedPointer<File>(new File(url.toLocalFile())));
        }
    }
    else if (mimeData->hasImage())
    {
        QSharedPointer<File> file(new File);

        if (!file->open(QIODevice::NewOnly | QIODevice::ReadWrite))
        {
            ui->chatBrowser->append(tr("Unable to create temporary file"));
            return;
        }

        if (!qvariant_cast<QImage>(mimeData->imageData()).save(file.data(), "jpg"))
        {
            ui->chatBrowser->append(tr("Unable to save image to temporary file"));
            return;
        }

        sendFile(file);
    }
}

void MainWindow::sendFile(const QSharedPointer<File> &file)
{
    if (!file->isOpen())
    {
        if (!file->open(QIODevice::ReadOnly))
        {
            ui->chatBrowser->append(tr("Unable to open file"));
            return;
        }
    }
    else
    {
        file->seek(0);
    }

    if (file->isSequential())
    {
        ui->chatBrowser->append(tr("You can't send a sequential file"));
        return;
    }

    if (file->size() < 1)
    {
        ui->chatBrowser->append(tr("You can't send an empty file"));
        return;
    }

    auto d = new TransferDialog(this, server, file);

    connect(d, &TransferDialog::completed, this, [ = ]
    {
        onFileSent(file);
    });

    QMetaObject::invokeMethod(server, "sendFile",
                              Q_ARG(QSharedPointer<File>, file));
}

void MainWindow::sendMessage(const QString &message)
{
    auto dt = QDateTime::currentDateTime();

    ui->lineEdit->clear();
    ui->chatBrowser->append(message, server->getUsername(), dt);

    QMetaObject::invokeMethod(server, "sendMessage",
                              Q_ARG(qint64, dt.toSecsSinceEpoch()),
                              Q_ARG(QString, message));
}
