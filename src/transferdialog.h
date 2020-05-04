#ifndef TRANSFERDIALOG_H
#define TRANSFERDIALOG_H

#include <QDialog>

namespace Ui {
class TransferDialog;
}

class File;
class Server;
class TransferDialog : public QDialog
{
    Q_OBJECT
public:
    explicit TransferDialog(QWidget *, Server *, const QSharedPointer<File> &);
    ~TransferDialog();

protected:
    void timerEvent(QTimerEvent *) override;

signals:
    void completed();

private slots:
    void onCancel();
    void onChunkTransferred(QByteArray, qint64);

private:
    Ui::TransferDialog *ui;

    Server *server;
    QSharedPointer<File> file;

    qint64 bytesTransferred = 0;
    qint64 elapsedTime = 0;
};

#endif // TRANSFERDIALOG_H
