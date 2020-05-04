#include "chatbrowser.h"
#include "core/client.h"

#include <QUrlQuery>

ChatBrowser::ChatBrowser(QWidget *parent) : QTextBrowser(parent)
{
}

void ChatBrowser::append(QString message, const QString &sender, const QDateTime &dt)
{
    message = message.trimmed();

    auto i = re.globalMatch(message);

    while (i.hasNext())
    {
        auto match = i.next();

        QString customName;

        QUrl url(match.captured());

        if (url.scheme() == "neutron")
        {
            if (url.host() == "file")
            {
                QUrlQuery q(url);

                if (q.hasQueryItem("id")
                        && q.hasQueryItem("name")
                        && q.hasQueryItem("size"))
                {
                    customName = q.queryItemValue("name");
                }
            }
        }

        message = message.replace(match.captured(), QString("<a href=\"%1\">%2</a>")
                                  .arg(match.captured())
                                  .arg(customName.isEmpty()
                                       ? match.captured()
                                       : customName));
    }

    auto html = QString("<span>[%1] %2 %3</span>").arg(dt.toString("hh:mm:ss"));

    if (sender.isEmpty())
    {
        html = html
               .arg(message)
               .arg(QString());
    }
    else
    {
        html = html
               .arg("&lt;" + sender + "&gt;")
               .arg(message);
    }

    QTextBrowser::append(html);
    QTextBrowser::moveCursor(QTextCursor::End);
}
