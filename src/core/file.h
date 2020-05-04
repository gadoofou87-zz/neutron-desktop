#ifndef FILE_H
#define FILE_H

#include <QFile>

class File : public QFile
{
    Q_OBJECT
public:
    explicit File();
    explicit File(const QString &);
    ~File();

    bool isCancellationRequested() const;

    const QByteArray &getId() const;
    QString getName() const;
    qint64 getLastRead() const;
    qint64 getLastWritten() const;
    qint64 getRemained() const;

    void setId(const QByteArray &);

    void requestCancellation();

    QByteArray read();
    void write(const QByteArray &);

private:
    QByteArray id;
    qint64 lastRead = 0;
    qint64 lastWritten = 0;
    bool autoRemove = false;
    bool cancellationRequested = false;
};

#endif // FILE_H
