#include "ui/pages/users_page.h"

#include "ElaComboBox.h"
#include "ElaLineEdit.h"
#include "ElaPushButton.h"
#include "ElaTableView.h"
#include "backend/models.h"

#include <QBrush>
#include <QFont>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QLocale>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include <QVariant>
#include <QVBoxLayout>

namespace
{
QString planToText(Tariff plan)
{
    switch (plan)
    {
    case Tariff::NoDiscount:
        return QStringLiteral(u"标准计费");
    case Tariff::Pack30h:
        return QStringLiteral(u"30 小时套餐");
    case Tariff::Pack60h:
        return QStringLiteral(u"60 小时套餐");
    case Tariff::Pack150h:
        return QStringLiteral(u"150 小时套餐");
    case Tariff::Unlimited:
        return QStringLiteral(u"包月不限时");
    }
    return QStringLiteral(u"未知套餐");
}

QString roleToText(UserRole role)
{
    switch (role)
    {
    case UserRole::Admin:
        return QStringLiteral(u"管理员");
    case UserRole::User:
    default:
        return QStringLiteral(u"普通用户");
    }
}

QString statusToText(bool enabled)
{
    return enabled ? QStringLiteral(u"启用") : QStringLiteral(u"停用");
}
} // namespace

class UsersFilterProxyModel : public QSortFilterProxyModel
{
public:
    explicit UsersFilterProxyModel(QObject *parent = nullptr)
        : QSortFilterProxyModel(parent)
    {
        setFilterCaseSensitivity(Qt::CaseInsensitive);
    }

    void setSearchText(QString text)
    {
        if (m_searchText == text)
            return;
        m_searchText = std::move(text);
        invalidateFilter();
    }

    void setPlanFilter(int plan)
    {
        if (m_planFilter == plan)
            return;
        m_planFilter = plan;
        invalidateFilter();
    }

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override
    {
        if (m_planFilter >= 0)
        {
            const auto index = sourceModel()->index(sourceRow, 2, sourceParent);
            const int planValue = index.data(Qt::UserRole).toInt();
            if (planValue != m_planFilter)
                return false;
        }

        if (m_searchText.isEmpty())
            return true;

        for (int column = 0; column < sourceModel()->columnCount(); ++column)
        {
            const auto index = sourceModel()->index(sourceRow, column, sourceParent);
            if (index.data(Qt::DisplayRole).toString().contains(m_searchText, Qt::CaseInsensitive))
                return true;
        }
        return false;
    }

private:
    QString m_searchText;
    int m_planFilter{-1};
};

UsersPage::UsersPage(QWidget *parent)
    : BasePage(QStringLiteral(u"用户管理"),
               QStringLiteral(u"维护上网账号、套餐、权限及账户余额，所有更改可随时保存至数据文件。"),
               parent)
{
    setupToolbar();
    setupTable();
}

void UsersPage::setupToolbar()
{
    auto *toolbar = new QWidget(this);
    auto *layout = new QHBoxLayout(toolbar);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    m_searchEdit = new ElaLineEdit(this);
    m_searchEdit->setPlaceholderText(QStringLiteral(u"搜索姓名或账号…"));
    m_searchEdit->setClearButtonEnabled(true);

    m_planFilterCombo = new ElaComboBox(this);
    m_planFilterCombo->addItem(QStringLiteral(u"全部套餐"), QVariant::fromValue(-1));
    m_planFilterCombo->addItem(QStringLiteral(u"标准计费"), QVariant::fromValue(static_cast<int>(Tariff::NoDiscount)));
    m_planFilterCombo->addItem(QStringLiteral(u"30 小时套餐"), QVariant::fromValue(static_cast<int>(Tariff::Pack30h)));
    m_planFilterCombo->addItem(QStringLiteral(u"60 小时套餐"), QVariant::fromValue(static_cast<int>(Tariff::Pack60h)));
    m_planFilterCombo->addItem(QStringLiteral(u"150 小时套餐"), QVariant::fromValue(static_cast<int>(Tariff::Pack150h)));
    m_planFilterCombo->addItem(QStringLiteral(u"包月不限时"), QVariant::fromValue(static_cast<int>(Tariff::Unlimited)));
    m_planFilterCombo->setCurrentIndex(0);

    m_addButton = new ElaPushButton(QStringLiteral(u"新增"), this);
    m_editButton = new ElaPushButton(QStringLiteral(u"编辑"), this);
    m_deleteButton = new ElaPushButton(QStringLiteral(u"删除"), this);
    m_reloadButton = new ElaPushButton(QStringLiteral(u"重新加载"), this);
    m_saveButton = new ElaPushButton(QStringLiteral(u"保存变更"), this);

    layout->addWidget(m_searchEdit, 1);
    layout->addWidget(m_planFilterCombo);
    layout->addSpacing(12);
    layout->addWidget(m_addButton);
    layout->addWidget(m_editButton);
    layout->addWidget(m_deleteButton);
    layout->addSpacing(12);
    layout->addWidget(m_reloadButton);
    layout->addWidget(m_saveButton);

    bodyLayout()->addWidget(toolbar);

    connect(m_searchEdit, &ElaLineEdit::textChanged, this, &UsersPage::applyFilter);
    connect(m_planFilterCombo, &ElaComboBox::currentIndexChanged, this, [this]
            { applyFilter(m_searchEdit->text()); });

    connect(m_addButton, &ElaPushButton::clicked, this, [this]
            { emit requestCreateUser(); });
    connect(m_editButton, &ElaPushButton::clicked, this, [this]
            {
                const auto accounts = selectedAccounts();
                if (!accounts.isEmpty())
                    emit requestEditUser(accounts.first());
            });
    connect(m_deleteButton, &ElaPushButton::clicked, this, [this]
            {
                const auto accounts = selectedAccounts();
                if (!accounts.isEmpty())
                    emit requestDeleteUsers(accounts);
            });
    connect(m_reloadButton, &ElaPushButton::clicked, this, &UsersPage::requestReloadUsers);
    connect(m_saveButton, &ElaPushButton::clicked, this, &UsersPage::requestSaveUsers);
}

void UsersPage::setupTable()
{
    m_model = std::make_unique<QStandardItemModel>(0, 6, this);
    m_model->setHeaderData(0, Qt::Horizontal, QStringLiteral(u"账号"));
    m_model->setHeaderData(1, Qt::Horizontal, QStringLiteral(u"姓名"));
    m_model->setHeaderData(2, Qt::Horizontal, QStringLiteral(u"套餐"));
    m_model->setHeaderData(3, Qt::Horizontal, QStringLiteral(u"角色"));
    m_model->setHeaderData(4, Qt::Horizontal, QStringLiteral(u"状态"));
    m_model->setHeaderData(5, Qt::Horizontal, QStringLiteral(u"余额"));

    m_proxyModel = std::unique_ptr<QSortFilterProxyModel>(new UsersFilterProxyModel(this));
    m_proxyModel->setSourceModel(m_model.get());

    m_table = new ElaTableView(this);
    m_table->setModel(m_proxyModel.get());
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);
    m_table->setAlternatingRowColors(true);

    bodyLayout()->addWidget(m_table, 1);
}

void UsersPage::setUsers(const std::vector<User> &users)
{
    m_model->removeRows(0, m_model->rowCount());
    m_model->setRowCount(static_cast<int>(users.size()));

    const QLocale locale(QLocale::Chinese, QLocale::China);
    for (int row = 0; row < static_cast<int>(users.size()); ++row)
    {
        const auto &user = users[static_cast<std::size_t>(row)];

        auto *accountItem = new QStandardItem(user.account);
        accountItem->setEditable(false);
        if (!m_currentAccount.isEmpty() && user.account.compare(m_currentAccount, Qt::CaseInsensitive) == 0)
        {
            QFont font = accountItem->font();
            font.setBold(true);
            accountItem->setFont(font);
        }
        m_model->setItem(row, 0, accountItem);

        auto *nameItem = new QStandardItem(user.name);
        nameItem->setEditable(false);
        m_model->setItem(row, 1, nameItem);

        auto *planItem = new QStandardItem(planToText(user.plan));
        planItem->setData(static_cast<int>(user.plan), Qt::UserRole);
        planItem->setEditable(false);
        m_model->setItem(row, 2, planItem);

        auto *roleItem = new QStandardItem(roleToText(user.role));
        roleItem->setEditable(false);
        m_model->setItem(row, 3, roleItem);

        auto *statusItem = new QStandardItem(statusToText(user.enabled));
        statusItem->setEditable(false);
        if (!user.enabled)
        {
            statusItem->setForeground(QBrush(Qt::red));
        }
        m_model->setItem(row, 4, statusItem);

        const QString balanceText = locale.toString(user.balance, 'f', 2);
        auto *balanceItem = new QStandardItem(balanceText);
        balanceItem->setEditable(false);
        if (user.balance < 0)
            balanceItem->setForeground(QBrush(Qt::red));
        m_model->setItem(row, 5, balanceItem);
    }
}

void UsersPage::setAdminMode(bool adminMode)
{
    m_adminMode = adminMode;
    m_addButton->setVisible(m_adminMode);
    m_editButton->setVisible(m_adminMode);
    m_deleteButton->setVisible(m_adminMode);
    m_reloadButton->setVisible(m_adminMode);
    m_saveButton->setVisible(m_adminMode);
    m_planFilterCombo->setEnabled(true);
    m_searchEdit->setEnabled(true);
    if (!m_adminMode)
    {
        // 普通用户不允许批量选择
        m_table->setSelectionMode(QAbstractItemView::NoSelection);
    }
    else
    {
        m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    }
}

void UsersPage::setCurrentAccount(const QString &account)
{
    m_currentAccount = account;
}

QStringList UsersPage::selectedAccounts() const
{
    QStringList accounts;
    if (!m_table->selectionModel())
        return accounts;

    const auto selectedRows = m_table->selectionModel()->selectedRows();
    for (const auto &index : selectedRows)
    {
        accounts.append(index.sibling(index.row(), 0).data().toString());
    }
    accounts.removeDuplicates();
    return accounts;
}

void UsersPage::clearSelection()
{
    if (m_table->selectionModel())
        m_table->selectionModel()->clear();
}

void UsersPage::applyFilter(const QString &text)
{
    if (!m_proxyModel)
        return;
    auto *proxy = static_cast<UsersFilterProxyModel *>(m_proxyModel.get());
    proxy->setSearchText(text);
    const int plan = m_planFilterCombo->currentData().toInt();
    proxy->setPlanFilter(plan);
}
