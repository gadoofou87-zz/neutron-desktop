#ifndef CONNECTDIALOG_H
#define CONNECTDIALOG_H

#include <QDialog>
#include <QTreeWidgetItem>

namespace Ui {
class ConnectDialog;
}

class ConnectDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ConnectDialog(QWidget * = nullptr);
    ~ConnectDialog();

signals:
    void dataEntered(QString,
                     QString, int,
                     QString, QString, bool,
                     QString, int,
                     QString, QString);

private slots:
    void onAccepted();
    void onClicked();
    void onItemClicked(const QTreeWidgetItem *);
    void onItemSelectionChanged();
    void onTextChanged();
    void onTextEdited(const QString &);
    void onToggled(bool);

private:
    Ui::ConnectDialog *ui;

    void clear();
};

#endif // CONNECTDIALOG_H
