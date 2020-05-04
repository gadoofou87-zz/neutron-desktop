#include "file.h"
#include "client.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>

static constexpr qint64 PAGE_SIZE = 32768;

File::File()
{
    setFileName(QString("%1/image_%2.jpg")
                .arg(QDir::tempPath())
                .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm-ss")));
    autoRemove = true;
}

File::File(const QString &name) : QFile(name)
{
}

File::~File()
{
    if (isOpen())
    {
        if (isWritable())
        {
            if (!atEnd() || autoRemove)
            {
                remove();
            }
        }

        close();
    }
}

bool File::isCancellationRequested() const
{
    return cancellationRequested;
}

const QByteArray &File::getId() const
{
    return id;
}

QString File::getName() const
{
    return QFileInfo(*this).fileName();
}

qint64 File::getLastRead() const
{
    return lastRead;
}

qint64 File::getLastWritten() const
{
    return lastWritten;
}

qint64 File::getRemained() const
{
    return size() - pos();
}

void File::setId(const QByteArray &id)
{
    this->id = id;
}

void File::requestCancellation()
{
    cancellationRequested = true;
}

QByteArray File::read()
{
    QByteArray data;
    data.resize(qMin(getRemained(), PAGE_SIZE));

    lastRead = QIODevice::read(data.data(), data.size());

    if (qint64(data.size()) != lastRead)
    {
        Client::error(tr("Error reading from file"));
    }

    return data;
}

void File::write(const QByteArray &data)
{
    lastWritten = QIODevice::write(data.constData(), data.size());

    if (qint64(data.size()) != lastWritten)
    {
        Client::error(tr("Error writing to file"));
    }
}
