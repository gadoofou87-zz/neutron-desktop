#include "transferdialog.h"
#include "ui_transferdialog.h"
#include "core/file.h"
#include "core/server.h"

TransferDialog::TransferDialog(QWidget *parent, Server *server, const QSharedPointer<File> &file)
    : QDialog(parent)
    , ui(new Ui::TransferDialog)
    , server(server)
    , file(file)
{
    ui->setupUi(this);
    ui->label_2->setText(file->isWritable()
                         ? tr("Receiving")
                         : tr("Sending"));
    ui->label_4->setText(file->getName());

    connect(ui->pushButton, &QPushButton::clicked,
            this, &TransferDialog::onCancel);
    connect(server, &Server::chunkTransferred,
            this, &TransferDialog::onChunkTransferred);
    connect(server, &QObject::destroyed,
            this, &QObject::deleteLater);

    startTimer(1000);

    show();
}

TransferDialog::~TransferDialog()
{
    delete ui;
}

void TransferDialog::timerEvent(QTimerEvent *)
{
    ui->label_6->setText(locale().formattedDataSize(bytesTransferred / ++elapsedTime) + "/s");
}

void TransferDialog::onCancel()
{
    QMetaObject::invokeMethod(server, "cancelTransfer",
                              Q_ARG(QByteArray, file->getId()));
    deleteLater();
}

void TransferDialog::onChunkTransferred(QByteArray id, qint64 sz)
{
    if (file->getId() != id)
    {
        return;
    }

    bytesTransferred += sz;

    ui->progressBar->setValue((bytesTransferred * 100) / file->size());

    if (bytesTransferred != file->size())
    {
        return;
    }

    emit completed();
    deleteLater();
}
