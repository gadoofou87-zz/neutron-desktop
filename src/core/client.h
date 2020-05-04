#ifndef CLIENT_H
#define CLIENT_H

#include <QSettings>
#include <QThread>

class Client : public QObject
{
    Q_OBJECT
public:
    explicit Client();

    static QSettings &getSettings();
    static QThread *getWorkerThread();

    [[ noreturn ]] static void error(const QString &);

private:
    static QSettings settings;
    static QThread *workerThread;

    static void initCrypto();
    static void initDatabase();
};

#endif // CLIENT_H
