#ifndef HISTORYFORM_H
#define HISTORYFORM_H

#include <QTreeWidgetItem>
#include <QUrl>
#include <QWidget>

namespace Ui {
class HistoryForm;
}

class HistoryForm : public QWidget
{
    Q_OBJECT
public:
    explicit HistoryForm(QWidget * = nullptr);
    ~HistoryForm();

private slots:
    void onAnchorClicked(QUrl);
    void onItemClicked(const QTreeWidgetItem *);

private:
    Ui::HistoryForm *ui;

    QByteArray id_room;
    QByteArray id_server;

    void refresh();
};

#endif // HISTORYFORM_H
