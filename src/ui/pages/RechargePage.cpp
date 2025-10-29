#include "ui/pages/RechargePage.h"

#include "ElaComboBox.h"
#include "ElaLineEdit.h"
#include "ElaPushButton.h"
#include "ElaTableView.h"
#include "ElaText.h"
#include "backend/Models.h"
#include "ui/ThemeUtils.h"

#include <QBrush>
#include <QDateTime>
#include <QDoubleSpinBox>
#include <QStandardItemModel>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLocale>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QVBoxLayout>
#include <QTimer>

RechargePage::RechargePage(QWidget *parent)
    : BasePage(QStringLiteral(u"充值与余额管理"),
               QStringLiteral(u"查看账户余额、执行充值并检索历史记录。"),
               parent)
{
    setupToolbar();
    setupTable();
}

void RechargePage::setupToolbar()
{
    auto *toolbar = new QWidget(this);
    auto *layout = new QHBoxLayout(toolbar);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    m_accountCombo = new ElaComboBox(this);
    m_accountCombo->setPlaceholderText(QStringLiteral(u"选择账号"));

    m_amountSpin = new QDoubleSpinBox(this);
    m_amountSpin->setRange(-1000000.0, 1000000.0);
    m_amountSpin->setDecimals(2);
    m_amountSpin->setValue(50.0);
    m_amountSpin->setSuffix(QStringLiteral(u" 元"));

    m_noteEdit = new ElaLineEdit(this);
    m_noteEdit->setPlaceholderText(QStringLiteral(u"备注，可选"));

    m_balanceLabel = new ElaText(this);
    m_balanceLabel->setTextPixelSize(13);
    m_balanceLabel->setWordWrap(true);

    m_rechargeButton = new ElaPushButton(QStringLiteral(u"确认操作"), this);

    layout->addWidget(new ElaText(QStringLiteral(u"充值账号"), this));
    layout->addWidget(m_accountCombo, 1);
    layout->addWidget(new ElaText(QStringLiteral(u"金额"), this));
    layout->addWidget(m_amountSpin);
    layout->addWidget(m_noteEdit, 1);
    layout->addWidget(m_rechargeButton);

    bodyLayout()->addWidget(toolbar);
    bodyLayout()->addWidget(m_balanceLabel);

    connect(m_rechargeButton, &ElaPushButton::clicked, this, [this]
            {
                const QString account = selectedAccount();
                const double amount = m_amountSpin->value();
                if (account.isEmpty())
                    return;
                emit rechargeRequested(account, amount, m_noteEdit->text().trimmed(), !m_adminMode); });
}

void RechargePage::setupTable()
{
    m_model = std::make_unique<QStandardItemModel>(0, 7, this);
    m_model->setHeaderData(0, Qt::Horizontal, QStringLiteral(u"时间"));
    m_model->setHeaderData(1, Qt::Horizontal, QStringLiteral(u"账号"));
    m_model->setHeaderData(2, Qt::Horizontal, QStringLiteral(u"姓名"));
    m_model->setHeaderData(3, Qt::Horizontal, QStringLiteral(u"类型"));
    m_model->setHeaderData(4, Qt::Horizontal, QStringLiteral(u"金额(元)"));
    m_model->setHeaderData(5, Qt::Horizontal, QStringLiteral(u"余额(元)"));
    m_model->setHeaderData(6, Qt::Horizontal, QStringLiteral(u"操作人/备注"));

    m_table = new ElaTableView(this);
    m_table->setModel(m_model.get());
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    auto *header = m_table->horizontalHeader();
    header->setSectionResizeMode(QHeaderView::Interactive);
    header->setStretchLastSection(false);
    m_table->setAlternatingRowColors(true);

    bodyLayout()->addWidget(m_table, 1);
}

void RechargePage::setAdminMode(bool adminMode)
{
    m_adminMode = adminMode;
    m_accountCombo->setEnabled(m_adminMode);
    m_accountCombo->setVisible(m_adminMode);
    if (!m_adminMode)
    {
        m_noteEdit->setPlaceholderText(QStringLiteral(u"可填写备注"));
        m_rechargeButton->setText(QStringLiteral(u"立即充值"));
        m_amountSpin->setMinimum(0.01);
    }
    else
    {
        m_rechargeButton->setText(QStringLiteral(u"保存流水"));
        m_amountSpin->setMinimum(-1000000.0);
    }
}

void RechargePage::setUsers(const std::vector<User> &users)
{
    m_accountNames.clear();
    m_accountCombo->clear();
    for (const auto &user : users)
    {
        m_accountNames.insert(user.account, user.name);
        if (m_adminMode)
            m_accountCombo->addItem(QStringLiteral("%1 (%2)").arg(user.account, user.name), user.account);
    }
}

void RechargePage::setRechargeRecords(const std::vector<RechargeRecord> &records)
{
    m_model->removeRows(0, m_model->rowCount());
    m_model->setRowCount(static_cast<int>(records.size()));

    const QLocale locale(QLocale::Chinese, QLocale::China);
    for (int row = 0; row < static_cast<int>(records.size()); ++row)
    {
        const auto &record = records[static_cast<std::size_t>(row)];
        const QString name = m_accountNames.value(record.account);
        const QString typeText = record.amount >= 0 ? QStringLiteral(u"充值") : QStringLiteral(u"扣费");

        auto *timeItem = new QStandardItem(record.timestamp.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
        timeItem->setEditable(false);
        m_model->setItem(row, 0, timeItem);

        auto *accountItem = new QStandardItem(record.account);
        accountItem->setEditable(false);
        m_model->setItem(row, 1, accountItem);

        auto *nameItem = new QStandardItem(name);
        nameItem->setEditable(false);
        m_model->setItem(row, 2, nameItem);

        auto *typeItem = new QStandardItem(typeText);
        typeItem->setEditable(false);
        m_model->setItem(row, 3, typeItem);

        const QString amountText = locale.toString(record.amount, 'f', 2);
        auto *amountItem = new QStandardItem(amountText);
        amountItem->setEditable(false);
        if (record.amount < 0)
            amountItem->setForeground(QBrush(Qt::red));
        m_model->setItem(row, 4, amountItem);

        auto *balanceItem = new QStandardItem(locale.toString(record.balanceAfter, 'f', 2));
        balanceItem->setEditable(false);
        if (record.balanceAfter < 0)
            balanceItem->setForeground(QBrush(Qt::red));
        m_model->setItem(row, 5, balanceItem);

        QString detail = record.operatorAccount;
        if (!record.note.isEmpty())
        {
            if (!detail.isEmpty())
                detail.append(QStringLiteral(" / "));
            detail.append(record.note);
        }
        auto *noteItem = new QStandardItem(detail);
        noteItem->setEditable(false);
        m_model->setItem(row, 6, noteItem);
    }

    if (m_table)
        resizeTableToFit(m_table);
}

void RechargePage::reloadPageData()
{
    if (!m_table)
        return;

    QTimer::singleShot(0, this, [this]
                       { resizeTableToFit(m_table); });
}

void RechargePage::setCurrentAccount(const QString &account)
{
    m_currentAccount = account;
    if (!m_adminMode)
    {
        m_accountCombo->clear();
        const QString displayName = m_accountNames.value(account, account);
        m_accountCombo->addItem(QStringLiteral("%1 (%2)").arg(account, displayName), account);
        m_accountCombo->setCurrentIndex(0);
    }
}

void RechargePage::setCurrentBalance(double balance)
{
    const QLocale locale(QLocale::Chinese, QLocale::China);
    const QString text = QStringLiteral(u"当前余额：%1 元").arg(locale.toString(balance, 'f', 2));
    m_balanceLabel->setText(text);
    if (balance < 0)
        m_balanceLabel->setText(QStringLiteral(u"%1（已欠费，请尽快充值）").arg(text));
    else if (balance < 20)
        m_balanceLabel->setText(QStringLiteral(u"%1（余额即将不足，请关注）").arg(text));
}

QString RechargePage::selectedAccount() const
{
    if (m_adminMode)
        return m_accountCombo->currentData().toString();
    return m_currentAccount;
}
