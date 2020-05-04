#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPointer>
#include <QTcpSocket>
#include <QTreeWidgetItem>
#include <QUrl>

namespace Ui {
class MainWindow;
}

class File;
class Server;
class HistoryForm;
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget * = nullptr);
    ~MainWindow() override;

protected:
    bool eventFilter(QObject *, QEvent *) override;

private slots:
    void onAnchorClicked(const QUrl &);
    void onItemDoubleClicked(const QTreeWidgetItem *);
    void onReturnPressed();

    // menuServer
    void onConnect();
    void onDisconnect();
    void onLeaveRoom();

    // menuSettings
    // menuView
    void onHistory();

    void onFileReceived(QSharedPointer<File>);
    void onFileSent(QSharedPointer<File>);
    void onInsertRoom(QByteArray, QString);
    void onLeftRoom();
    void onMessageReceived(QDateTime, QString, QString);
    void onParticipantJoined(QString);
    void onParticipantLeft(QString);
    void onPrint(QString);
    void onSetName(QString);

private:
    Ui::MainWindow *ui;
    QTreeWidgetItem *root;

    QPointer<QTcpSocket> socket;
    QPointer<Server> server;

    bool roomParticipant = false;

    const int PREVIEW_SIZE = 300;

    bool check(bool, bool);

    void connectToHost(const QString &,
                       const QString &, int,
                       const QString &, const QString &, bool,
                       const QString &, int,
                       const QString &, const QString &);
    void sendFile(const QMimeData *);
    void sendFile(const QSharedPointer<File> &);
    void sendMessage(const QString &);

    friend class HistoryForm;
};

#endif // MAINWINDOW_H
