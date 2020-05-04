#include "client.h"

#include <QApplication>
#include <QDir>
#include <QMessageBox>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>

#include <oqs/oqs.h>

QSettings Client::settings
{
    "client.ini",
    QSettings::IniFormat
};
QThread *Client::workerThread = new QThread;

Client::Client()
{
    #ifndef QT_DEBUG

    if (!QDir().mkpath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)))
    {
        error("Unable to create appdata folder");
    }

    QDir::setCurrent(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
    #endif

    initCrypto();
    initDatabase();

    workerThread->start();

    connect(qApp, &QApplication::aboutToQuit, [ = ]
    {
        workerThread->quit();
        workerThread->wait();
    });
}

QSettings &Client::getSettings()
{
    return settings;
}

QThread *Client::getWorkerThread()
{
    return workerThread;
}

void Client::error(const QString &reason)
{
    QMessageBox::critical(nullptr,
                          nullptr,
                          reason,
                          QMessageBox::Abort);
    exit(EXIT_FAILURE);
}

void Client::initCrypto()
{
    #if !defined (OQS_ENABLE_KEM_sidh_p751) || !defined (OQS_ENABLE_SIG_picnic2_L5_FS)
    error("The required post quantum algorithms are not available for use");
    #endif
}

void Client::initDatabase()
{
    if (!QSqlDatabase::isDriverAvailable("QSQLITE"))
    {
        error("Database driver not found");
    }

    auto db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName("client.db");

    if (!db.open())
    {
        error("Could not open database");
    }

    QSqlQuery query;

    if (!query.exec("CREATE TABLE IF NOT EXISTS ARCHIVE"
                    "("
                    "ID INTEGER PRIMARY KEY AUTOINCREMENT,"
                    "TIMESTAMP  BIGINT  NOT NULL,"
                    "ID_SERVER  BLOB    NOT NULL,"
                    "ID_MESSAGE BLOB    NOT NULL,"
                    "ID_ROOM    BLOB    NOT NULL,"
                    "ID_SENDER  TEXT    NOT NULL,"
                    "CONTENT    TEXT    NOT NULL"
                    ")")
            || !query.exec("CREATE TABLE IF NOT EXISTS ROOMS"
                           "("
                           "ID        BLOB NOT NULL,"
                           "ID_SERVER BLOB NOT NULL,"
                           "NAME      TEXT NOT NULL,"
                           "PRIMARY KEY (ID, ID_SERVER)"
                           ")")
            || !query.exec("CREATE TABLE IF NOT EXISTS SERVERS"
                           "("
                           "ID   BLOB NOT NULL,"
                           "NAME TEXT NOT NULL,"
                           "PRIMARY KEY (ID)"
                           ")"))
    {
        error(query.lastError().text());
    }
}
