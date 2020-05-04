#include "connectdialog.h"
#include "ui_connectdialog.h"
#include "core/client.h"

ConnectDialog::ConnectDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ConnectDialog)
{
    ui->setupUi(this);
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);

    Client::getSettings().beginGroup("Servers");

    for (auto label : Client::getSettings().childGroups())
    {
        (new QTreeWidgetItem(ui->treeWidget))->setText(0, label);
    }

    Client::getSettings().endGroup();

    connect(ui->buttonBox, &QDialogButtonBox::accepted,
            this, &ConnectDialog::onAccepted);
    connect(ui->pushButton, &QPushButton::clicked,
            this, &ConnectDialog::onClicked);
    connect(ui->treeWidget, &QTreeWidget::itemClicked,
            this, &ConnectDialog::onItemClicked);
    connect(ui->treeWidget, &QTreeWidget::itemSelectionChanged,
            this, &ConnectDialog::onItemSelectionChanged);
    connect(ui->lineEdit, &QLineEdit::textChanged,
            this, &ConnectDialog::onTextChanged);
    connect(ui->comboBox, &QComboBox::currentTextChanged,
            this, &ConnectDialog::onTextChanged);
    connect(ui->lineEdit_2, &QLineEdit::textChanged,
            this, &ConnectDialog::onTextChanged);
    connect(ui->lineEdit_3, &QLineEdit::textChanged,
            this, &ConnectDialog::onTextChanged);
    connect(ui->lineEdit_7, &QLineEdit::textEdited,
            this, &ConnectDialog::onTextEdited);
    connect(ui->checkBox_2, &QCheckBox::toggled,
            this, &ConnectDialog::onToggled);
}

ConnectDialog::~ConnectDialog()
{
    delete ui;
}

void ConnectDialog::onAccepted()
{
    auto server_id = ui->lineEdit->text();
    auto server_host = ui->comboBox->currentText();
    auto server_port = ui->spinBox->value();

    auto username = ui->lineEdit_2->text();
    auto password = ui->lineEdit_3->text();
    auto signup = ui->checkBox->isChecked();

    auto proxy_host = ui->lineEdit_4->text();
    auto proxy_port = ui->spinBox_2->value();
    auto proxy_user = ui->lineEdit_5->text();
    auto proxy_pass = ui->lineEdit_6->text();

    accept();

    emit dataEntered(server_id,
                     server_host, server_port,
                     username, password, signup,
                     proxy_host, proxy_port,
                     proxy_user, proxy_pass);

    if (!ui->checkBox_2->isChecked())
    {
        return;
    }

    Client::getSettings().beginGroup("Servers");
    Client::getSettings().beginGroup(ui->lineEdit_7->text());

    Client::getSettings().setValue("ID", server_id);
    Client::getSettings().setValue("Host", server_host);
    Client::getSettings().setValue("Port", server_port);
    Client::getSettings().setValue("Socks5Host", proxy_host);
    Client::getSettings().setValue("Socks5Port", proxy_pass);
    Client::getSettings().setValue("Socks5User", proxy_user);
    Client::getSettings().setValue("Socks5Pass", proxy_pass);

    Client::getSettings().endGroup();
    Client::getSettings().endGroup();
}

void ConnectDialog::onClicked()
{
    clear();

    auto item = ui->treeWidget->currentItem();

    Client::getSettings().beginGroup("Servers");
    Client::getSettings().beginGroup(item->text(0));
    Client::getSettings().remove(QString());
    Client::getSettings().endGroup();
    Client::getSettings().endGroup();

    delete item;
}

void ConnectDialog::onItemClicked(const QTreeWidgetItem *item)
{
    clear();

    Client::getSettings().beginGroup("Servers");
    Client::getSettings().beginGroup(item->text(0));

    ui->lineEdit->setText(Client::getSettings().value("ID").toString());
    ui->comboBox->setCurrentText(Client::getSettings().value("Host").toString());
    ui->spinBox->setValue(Client::getSettings().value("Port").toInt());
    ui->lineEdit_4->setText(Client::getSettings().value("Socks5Host").toString());
    ui->spinBox_2->setValue(Client::getSettings().value("Socks5Port").toInt());
    ui->lineEdit_5->setText(Client::getSettings().value("Socks5User").toString());
    ui->lineEdit_6->setText(Client::getSettings().value("Socks5Pass").toString());

    Client::getSettings().endGroup();
    Client::getSettings().endGroup();

    ui->lineEdit_7->setText(item->text(0));
}

void ConnectDialog::onItemSelectionChanged()
{
    if (ui->treeWidget->selectedItems().isEmpty())
    {
        ui->pushButton->setEnabled(false);
    }
    else
    {
        ui->pushButton->setEnabled(true);
    }
}

void ConnectDialog::onTextChanged()
{
    if (ui->lineEdit->text().isEmpty()
            || ui->comboBox->currentText().isEmpty()
            || ui->lineEdit_2->text().isEmpty()
            || ui->lineEdit_3->text().isEmpty())
    {
        ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    }
    else
    {
        ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(true);
    }
}

void ConnectDialog::onTextEdited(const QString &label)
{
    for (int i = 0; i < ui->treeWidget->topLevelItemCount(); i++)
    {
        auto item = ui->treeWidget->topLevelItem(i);

        if (item->text(0) == label)
        {
            ui->treeWidget->setCurrentItem(item);
            break;
        }
        else
        {
            ui->treeWidget->setCurrentItem(nullptr);
        }
    }
}

void ConnectDialog::onToggled(bool checked)
{
    ui->label_10->setEnabled(checked);
    ui->lineEdit_7->setEnabled(checked);
}

void ConnectDialog::clear()
{
    ui->lineEdit->clear();
    ui->comboBox->clear();
    ui->comboBox->clearEditText();
    ui->spinBox->setValue(0);
    ui->lineEdit_2->clear();
    ui->lineEdit_3->clear();
    ui->checkBox->setCheckState(Qt::Unchecked);
    ui->lineEdit_4->clear();
    ui->spinBox_2->setValue(0);
    ui->lineEdit_5->clear();
    ui->lineEdit_6->clear();
    ui->lineEdit_7->clear();
    ui->checkBox_2->setCheckState(Qt::Unchecked);
}
