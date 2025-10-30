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
#include <QDoubleValidator>
#include <QStandardItemModel>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLocale>
#include <QStandardItem>
#include <QVBoxLayout>
#include <QTimer>
#include <QStackedLayout>
#include <QWidget>
#include <QSizePolicy>
#include <QSignalBlocker>

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

    auto *accountLabel = new ElaText(QStringLiteral(u"充值账号"), this);
    layout->addWidget(accountLabel);

    auto *accountContainer = new QWidget(this);
    m_accountStack = new QStackedLayout(accountContainer);
    m_accountStack->setContentsMargins(0, 0, 0, 0);

    m_accountCombo = new ElaComboBox(accountContainer);
    m_accountCombo->setPlaceholderText(QStringLiteral(u"选择账号"));
    m_accountStack->addWidget(m_accountCombo);

    m_accountDisplay = new ElaText(accountContainer);
    m_accountDisplay->setTextPixelSize(13);
    m_accountDisplay->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    m_accountDisplay->setWordWrap(false);
    m_accountDisplay->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_accountStack->addWidget(m_accountDisplay);

    layout->addWidget(accountContainer, 1);

    m_accountSearch = new ElaLineEdit(this);
    m_accountSearch->setPlaceholderText(QStringLiteral(u"搜索账号或姓名"));
    m_accountSearch->setClearButtonEnabled(true);
    m_accountSearch->setVisible(false);
    layout->addWidget(m_accountSearch);

    m_amountEdit = new ElaLineEdit(this);
    m_amountEdit->setPlaceholderText(QStringLiteral(u"输入金额，最多两位小数"));
    m_amountEdit->setClearButtonEnabled(true);
    m_amountValidator = new QDoubleValidator(-1000000.0, 1000000.0, 2, m_amountEdit);
    m_amountValidator->setNotation(QDoubleValidator::StandardNotation);
    m_amountEdit->setValidator(m_amountValidator);
    setAmount(50.0);

    m_noteEdit = new ElaLineEdit(this);
    m_noteEdit->setPlaceholderText(QStringLiteral(u"备注，可选"));

    m_balanceLabel = new ElaText(this);
    m_balanceLabel->setTextPixelSize(13);
    m_balanceLabel->setWordWrap(true);

    m_rechargeButton = new ElaPushButton(QStringLiteral(u"确认操作"), this);

    layout->addWidget(new ElaText(QStringLiteral(u"金额"), this));
    layout->addWidget(m_amountEdit);
    layout->addWidget(m_noteEdit, 1);
    layout->addWidget(m_rechargeButton);

    bodyLayout()->addWidget(toolbar);
    bodyLayout()->addWidget(m_balanceLabel);

    connect(m_rechargeButton, &ElaPushButton::clicked, this, [this]
            {
                const QString account = selectedAccount();
                bool ok = false;
                const double amount = currentAmount(&ok);
                if (account.isEmpty() || !ok)
                    return;
                if (!m_adminMode && amount < 0.01)
                    return;
                emit rechargeRequested(account, amount, m_noteEdit->text().trimmed(), !m_adminMode); });

    if (m_accountSearch)
    {
        connect(m_accountSearch, &ElaLineEdit::textChanged, this, [this]
                { applyAccountFilter(); });
    }
}

void RechargePage::setupTable()
{
    m_model = std::make_unique<QStandardItemModel>(0, 8, this);
    m_model->setHeaderData(0, Qt::Horizontal, QStringLiteral(u"时间"));
    m_model->setHeaderData(1, Qt::Horizontal, QStringLiteral(u"账号"));
    m_model->setHeaderData(2, Qt::Horizontal, QStringLiteral(u"姓名"));
    m_model->setHeaderData(3, Qt::Horizontal, QStringLiteral(u"类型"));
    m_model->setHeaderData(4, Qt::Horizontal, QStringLiteral(u"金额(元)"));
    m_model->setHeaderData(5, Qt::Horizontal, QStringLiteral(u"余额(元)"));
    m_model->setHeaderData(6, Qt::Horizontal, QStringLiteral(u"操作人"));
    m_model->setHeaderData(7, Qt::Horizontal, QStringLiteral(u"备注"));

    m_table = new ElaTableView(this);
    m_table->setModel(m_model.get());
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    auto *header = m_table->horizontalHeader();
    header->setSectionResizeMode(QHeaderView::Fixed);
    header->setStretchLastSection(false);
    m_table->setAlternatingRowColors(true);
    enableAutoFitScaling(m_table);

    bodyLayout()->addWidget(m_table, 1);
}

void RechargePage::setAdminMode(bool adminMode)
{
    m_adminMode = adminMode;
    if (m_accountCombo)
        m_accountCombo->setEnabled(m_adminMode);
    if (m_accountStack)
        m_accountStack->setCurrentWidget(m_adminMode ? static_cast<QWidget *>(m_accountCombo)
                                                     : static_cast<QWidget *>(m_accountDisplay));
    if (m_accountSearch)
    {
        m_accountSearch->setVisible(m_adminMode);
        if (!m_adminMode)
            m_accountSearch->clear();
    }
    if (!m_adminMode)
    {
        m_noteEdit->setPlaceholderText(QStringLiteral(u"可填写备注"));
        m_rechargeButton->setText(QStringLiteral(u"立即充值"));
        if (m_amountEdit)
            m_amountEdit->setPlaceholderText(QStringLiteral(u"输入金额，最低 0.01 元"));
        if (m_amountValidator)
            m_amountValidator->setRange(0.01, 1000000.0, 2);
        bool ok = false;
        const double amount = currentAmount(&ok);
        if (!ok || amount < 0.01)
            setAmount(50.0);
        if (m_accountDisplay)
            m_accountDisplay->setToolTip(QString());
    }
    else
    {
        m_rechargeButton->setText(QStringLiteral(u"充值"));
        if (m_amountEdit)
            m_amountEdit->setPlaceholderText(QStringLiteral(u"输入金额，可填写负数进行扣费"));
        if (m_amountValidator)
            m_amountValidator->setRange(-1000000.0, 1000000.0, 2);
    }

    applyAccountFilter();
}

void RechargePage::setUsers(const std::vector<User> &users)
{
    m_accountNames.clear();
    m_accountList.clear();
    if (m_accountCombo)
        m_accountCombo->clear();
    for (const auto &user : users)
    {
        m_accountNames.insert(user.account, user.name);
        m_accountList.append(QPair<QString, QString>(user.account, user.name));
    }

    applyAccountFilter();
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

        auto *operatorItem = new QStandardItem(record.operatorAccount);
        operatorItem->setEditable(false);
        m_model->setItem(row, 6, operatorItem);

        auto *noteItem = new QStandardItem(record.note);
        noteItem->setEditable(false);
        m_model->setItem(row, 7, noteItem);
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
        if (m_accountCombo)
            m_accountCombo->clear();
        const QString displayName = m_accountNames.value(account, account);
        if (m_accountCombo)
        {
            m_accountCombo->addItem(QStringLiteral("%1 (%2)").arg(account, displayName), account);
            m_accountCombo->setCurrentIndex(0);
        }
        if (m_accountDisplay)
            m_accountDisplay->setText(QStringLiteral(u"%1 (%2)").arg(account, displayName));
    }
    else if (m_accountCombo)
    {
        const int index = m_accountCombo->findData(account);
        if (index >= 0)
            m_accountCombo->setCurrentIndex(index);
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
        return m_accountCombo ? m_accountCombo->currentData().toString() : QString();
    return m_currentAccount;
}

double RechargePage::currentAmount(bool *ok) const
{
    if (!m_amountEdit)
    {
        if (ok)
            *ok = false;
        return 0.0;
    }
    bool localOk = false;
    const double value = m_amountEdit->text().trimmed().toDouble(&localOk);
    if (ok)
        *ok = localOk;
    return localOk ? value : 0.0;
}

void RechargePage::setAmount(double amount)
{
    if (!m_amountEdit)
        return;
    m_amountEdit->setText(QString::number(amount, 'f', 2));
}

void RechargePage::applyAccountFilter()
{
    if (!m_accountCombo)
        return;

    const QString filter = (m_accountSearch && m_accountSearch->isVisible()) ? m_accountSearch->text().trimmed() : QString();
    const QString currentAccount = selectedAccount();

    const QSignalBlocker blocker(m_accountCombo);
    m_accountCombo->clear();

    if (m_adminMode)
    {
        const Qt::CaseSensitivity cs = Qt::CaseInsensitive;
        for (const auto &entry : m_accountList)
        {
            const QString &account = entry.first;
            const QString &name = entry.second;
            if (!filter.isEmpty())
            {
                if (!account.contains(filter, cs) && !name.contains(filter, cs))
                    continue;
            }
            m_accountCombo->addItem(QStringLiteral("%1 (%2)").arg(account, name), account);
        }
    }
    else if (!m_currentAccount.isEmpty())
    {
        const QString displayName = m_accountNames.value(m_currentAccount, m_currentAccount);
        m_accountCombo->addItem(QStringLiteral("%1 (%2)").arg(m_currentAccount, displayName), m_currentAccount);
    }

    int targetIndex = m_accountCombo->findData(currentAccount);
    if (targetIndex < 0 && m_accountCombo->count() > 0)
        targetIndex = 0;
    if (targetIndex >= 0)
        m_accountCombo->setCurrentIndex(targetIndex);

    if (m_adminMode && m_accountCombo->count() == 0)
        m_currentAccount.clear();
}
