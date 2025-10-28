#include "ui/pages/users_page.h"

#include "ElaComboBox.h"
#include "ElaLineEdit.h"
#include "ElaPushButton.h"
#include "ElaTableView.h"
#include "backend/models.h"

#include <QHeaderView>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include <QVariant>
#include <QVBoxLayout>

class UsersFilterProxyModel : public QSortFilterProxyModel
{
public:
    explicit UsersFilterProxyModel(QObject* parent = nullptr)
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
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override
    {
        if (m_planFilter >= 0)
        {
            const auto index = sourceModel()->index(sourceRow, 2, sourceParent);
            const int roleValue = index.data(Qt::UserRole).toInt();
            if (roleValue != m_planFilter)
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

namespace
{
QString planToString(Tariff plan)
{
    switch (plan)
    {
    case Tariff::NoDiscount:
        return QStringLiteral(u8"标准计费");
    case Tariff::Pack30h:
        return QStringLiteral(u8"30 小时套餐");
    case Tariff::Pack60h:
        return QStringLiteral(u8"60 小时套餐");
    case Tariff::Pack150h:
        return QStringLiteral(u8"150 小时套餐");
    case Tariff::Unlimited:
        return QStringLiteral(u8"包月不限时");
    }
    return QStringLiteral(u8"未知");
}
}

UsersPage::UsersPage(QWidget* parent)
    : BasePage(QStringLiteral(u8"用户管理"),
               QStringLiteral(u8"维护上网账号、所属资费方案等信息，所有变更可随时保存至数据文件。"),
               parent)
{
    setupToolbar();
    setupTable();
}

void UsersPage::setupToolbar()
{
    auto* toolbar = new QWidget(this);
    auto* layout = new QHBoxLayout(toolbar);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    m_searchEdit = new ElaLineEdit(this);
    m_searchEdit->setPlaceholderText(QStringLiteral(u8"搜索姓名或账号…"));
    m_searchEdit->setClearButtonEnabled(true);

    m_planFilterCombo = new ElaComboBox(this);
    m_planFilterCombo->addItem(QStringLiteral(u8"全部套餐"), QVariant::fromValue(-1));
    m_planFilterCombo->addItem(QStringLiteral(u8"标准计费"), QVariant::fromValue(static_cast<int>(Tariff::NoDiscount)));
    m_planFilterCombo->addItem(QStringLiteral(u8"30 小时套餐"), QVariant::fromValue(static_cast<int>(Tariff::Pack30h)));
    m_planFilterCombo->addItem(QStringLiteral(u8"60 小时套餐"), QVariant::fromValue(static_cast<int>(Tariff::Pack60h)));
    m_planFilterCombo->addItem(QStringLiteral(u8"150 小时套餐"), QVariant::fromValue(static_cast<int>(Tariff::Pack150h)));
    m_planFilterCombo->addItem(QStringLiteral(u8"包月不限时"), QVariant::fromValue(static_cast<int>(Tariff::Unlimited)));
    m_planFilterCombo->setCurrentIndex(0);

    m_addButton = new ElaPushButton(QStringLiteral(u8"新增"), this);
    m_editButton = new ElaPushButton(QStringLiteral(u8"编辑"), this);
    m_deleteButton = new ElaPushButton(QStringLiteral(u8"删除"), this);
    m_reloadButton = new ElaPushButton(QStringLiteral(u8"重新加载"), this);
    m_saveButton = new ElaPushButton(QStringLiteral(u8"保存变更"), this);

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
    connect(m_planFilterCombo, &ElaComboBox::currentIndexChanged, this, [this] { applyFilter(m_searchEdit->text()); });

    connect(m_addButton, &ElaPushButton::clicked, this, [this] {
        emit requestCreateUser();
    });
    connect(m_editButton, &ElaPushButton::clicked, this, [this] {
        const auto accounts = selectedAccounts();
        if (!accounts.isEmpty())
        {
            emit requestEditUser(accounts.first());
        }
    });
    connect(m_deleteButton, &ElaPushButton::clicked, this, [this] {
        const auto accounts = selectedAccounts();
        if (!accounts.isEmpty())
        {
            emit requestDeleteUsers(accounts);
        }
    });
    connect(m_reloadButton, &ElaPushButton::clicked, this, &UsersPage::requestReloadUsers);
    connect(m_saveButton, &ElaPushButton::clicked, this, &UsersPage::requestSaveUsers);
}

void UsersPage::setupTable()
{
    m_model = std::make_unique<QStandardItemModel>(0, 3, this);
    m_model->setHeaderData(0, Qt::Horizontal, QStringLiteral(u8"账号"));
    m_model->setHeaderData(1, Qt::Horizontal, QStringLiteral(u8"姓名"));
    m_model->setHeaderData(2, Qt::Horizontal, QStringLiteral(u8"套餐"));

    m_proxyModel = std::make_unique<UsersFilterProxyModel>(this);
    m_proxyModel->setSourceModel(m_model.get());

    m_table = new ElaTableView(this);
    m_table->setModel(m_proxyModel.get());
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setAlternatingRowColors(true);

    bodyLayout()->addWidget(m_table, 1);
}

void UsersPage::setUsers(const std::vector<User>& users)
{
    m_model->removeRows(0, m_model->rowCount());
    m_model->setRowCount(static_cast<int>(users.size()));

    for (int row = 0; row < static_cast<int>(users.size()); ++row)
    {
        const auto& user = users[static_cast<std::size_t>(row)];

        auto* accountItem = new QStandardItem(user.account);
        accountItem->setEditable(false);
        m_model->setItem(row, 0, accountItem);

        auto* nameItem = new QStandardItem(user.name);
        nameItem->setEditable(false);
        m_model->setItem(row, 1, nameItem);

        auto* planItem = new QStandardItem(planToString(user.plan));
        planItem->setData(static_cast<int>(user.plan), Qt::UserRole);
        planItem->setEditable(false);
        m_model->setItem(row, 2, planItem);
    }
}

QStringList UsersPage::selectedAccounts() const
{
    QStringList accounts;
    if (!m_table->selectionModel())
        return accounts;

    const auto selectedRows = m_table->selectionModel()->selectedRows();
    for (const auto& index : selectedRows)
    {
        accounts.append(index.sibling(index.row(), 0).data().toString());
    }
    accounts.removeDuplicates();
    return accounts;
}

void UsersPage::clearSelection()
{
    if (m_table->selectionModel())
    {
        m_table->selectionModel()->clear();
    }
}

void UsersPage::applyFilter(const QString& text)
{
    if (!m_proxyModel)
        return;
    m_proxyModel->setSearchText(text);
    const int plan = m_planFilterCombo->currentData().toInt();
    m_proxyModel->setPlanFilter(plan);
}
