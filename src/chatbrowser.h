#ifndef CHATBROWSER_H
#define CHATBROWSER_H

#include <QDateTime>
#include <QRegularExpression>
#include <QTextBrowser>

class ChatBrowser : public QTextBrowser
{
    Q_OBJECT
public:
    explicit ChatBrowser(QWidget * = nullptr);

public slots:
    void append(QString, const QString & = {},
                const QDateTime & = QDateTime::currentDateTime());

private:
    const QRegularExpression re { "((?:https?|ftp|neutron)://\\S+)" };
};

#endif // CHATBROWSER_H
