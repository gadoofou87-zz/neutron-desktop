#include "historyform.h"
#include "ui_historyform.h"
#include "mainwindow.h"
#include "core/client.h"

#include <QSqlError>
#include <QSqlQuery>

HistoryForm::HistoryForm(QWidget *parent)
    : QWidget(parent, Qt::Window)
    , ui(new Ui::HistoryForm)
{
    ui->setupUi(this);

    QSqlQuery query;
    query.prepare("SELECT *"
                  " FROM SERVERS");

    if (!query.exec())
    {
        Client::error(query.lastError().text());
    }

    while (query.next())
    {
        auto root = new QTreeWidgetItem(ui->treeWidget);
        root->setData(0, Qt::UserRole, query.value(0));
        root->setText(0, query.value(1).toString());

        QSqlQuery query;
        query.prepare("SELECT ID, NAME"
                      " FROM ROOMS"
                      " WHERE ID_SERVER = ?");
        query.addBindValue(root->data(0, Qt::UserRole));

        if (!query.exec())
        {
            Client::error(query.lastError().text());
        }

        while (query.next())
        {
            auto child = new QTreeWidgetItem(root);
            child->setData(0, Qt::UserRole, query.value(0));
            child->setText(0, query.value(1).toString());
        }
    }

    connect(ui->chatBrowser, &ChatBrowser::anchorClicked,
            this, &HistoryForm::onAnchorClicked);
    connect(ui->calendarWidget, &QCalendarWidget::clicked,
            this, &HistoryForm::refresh);
    connect(ui->checkBox, &QCheckBox::stateChanged,
            this, &HistoryForm::refresh);
    connect(ui->lineEdit, &QLineEdit::textChanged,
            this, &HistoryForm::refresh);
    connect(ui->treeWidget, &QTreeWidget::itemClicked,
            this, &HistoryForm::onItemClicked);

    show();
}

HistoryForm::~HistoryForm()
{
    delete ui;
}

void HistoryForm::onAnchorClicked(QUrl url)
{
    if (url.scheme() == "neutron")
    {
        url.setFragment(id_server.toHex());
    }

    qobject_cast<MainWindow *>(parent())->onAnchorClicked(url);
}

void HistoryForm::onItemClicked(const QTreeWidgetItem *item)
{
    if (item->parent())
    {
        id_room = item->data(0, Qt::UserRole).toByteArray();
        id_server = item->parent()->data(0, Qt::UserRole).toByteArray();
    }
    else
    {
        id_room.clear();
        id_server.clear();
    }

    refresh();
}

void HistoryForm::refresh()
{
    ui->chatBrowser->clear();

    if (id_room.isEmpty() || id_server.isEmpty())
    {
        return;
    }

    QSqlQuery query;
    query.prepare("SELECT TIMESTAMP, ID_SENDER, CONTENT"
                  " FROM ARCHIVE"
                  " WHERE ID_SERVER = ?"
                  " AND ID_ROOM = ?"
                  " AND TIMESTAMP > ?"
                  " AND TIMESTAMP < ?"
                  " AND CONTENT LIKE ?");
    query.addBindValue(id_server);
    query.addBindValue(id_room);

    auto searchQuery = ui->lineEdit->text();
    auto selectedDate = ui->calendarWidget->selectedDate();

    if (searchQuery.isEmpty() || ui->checkBox->isChecked())
    {
        query.addBindValue(selectedDate.startOfDay().toSecsSinceEpoch());
        query.addBindValue(selectedDate.endOfDay().toSecsSinceEpoch());
    }
    else
    {
        query.addBindValue(0);
        query.addBindValue(QDateTime::currentDateTime().toSecsSinceEpoch());
    }

    query.addBindValue(QString("%%1%").arg(searchQuery));

    if (!query.exec())
    {
        Client::error(query.lastError().text());
    }

    while (query.next())
    {
        ui->chatBrowser->append(query.value(2).toString(),
                                query.value(1).toString(),
                                QDateTime::fromSecsSinceEpoch(query.value(0).toLongLong()));
    }
}
