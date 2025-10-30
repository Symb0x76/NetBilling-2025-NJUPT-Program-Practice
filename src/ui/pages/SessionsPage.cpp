#include "ui/pages/SessionsPage.h"

#include "ElaComboBox.h"
#include "ElaLineEdit.h"
#include "ElaPushButton.h"
#include "ElaTableView.h"
#include "backend/Models.h"
#include "ui/ThemeUtils.h"

#include <QTimer>
#include <QDateTime>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include <QVariant>
#include <QVBoxLayout>

#include <algorithm>
#include <iterator>

namespace
{
    enum SessionRoles
    {
        AccountRole = Qt::UserRole + 1,
        BeginRole,
        EndRole,
        MinutesRole,
        NameRole
    };

    enum class ScopeFilter
    {
        All = 0,
        CrossMonth,
        CrossYear
    };
} // namespace

class SessionsFilterProxyModel : public QSortFilterProxyModel
{
public:
    explicit SessionsFilterProxyModel(QObject *parent = nullptr)
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

    void setScopeFilter(ScopeFilter scope)
    {
        if (m_scopeFilter == scope)
            return;
        m_scopeFilter = scope;
        invalidateFilter();
    }

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override
    {
        const auto begin = sourceModel()->index(sourceRow, 2, sourceParent).data(BeginRole).toDateTime();
        const auto end = sourceModel()->index(sourceRow, 3, sourceParent).data(EndRole).toDateTime();

        switch (m_scopeFilter)
        {
        case ScopeFilter::All:
            break;
        case ScopeFilter::CrossMonth:
            if (begin.date().year() == end.date().year() && begin.date().month() == end.date().month())
                return false;
            break;
        case ScopeFilter::CrossYear:
            if (begin.date().year() == end.date().year())
                return false;
            break;
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
    ScopeFilter m_scopeFilter{ScopeFilter::All};
};

SessionsPage::SessionsPage(QWidget *parent)
    : BasePage(QStringLiteral(u"上网记录"),
               QStringLiteral(u"管理详细的上网起止时间并支持手动录入、随机生成以及快速筛选。"),
               parent)
{
    setupToolbar();
    setupTable();
}

void SessionsPage::setupToolbar()
{
    auto *toolbar = new QWidget(this);
    auto *layout = new QHBoxLayout(toolbar);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    m_searchEdit = new ElaLineEdit(this);
    m_searchEdit->setPlaceholderText(QStringLiteral(u"搜索账号、姓名或时间…"));
    m_searchEdit->setClearButtonEnabled(true);

    m_scopeFilterCombo = new ElaComboBox(this);
    m_scopeFilterCombo->addItem(QStringLiteral(u"全部记录"), QVariant::fromValue(static_cast<int>(ScopeFilter::All)));
    m_scopeFilterCombo->addItem(QStringLiteral(u"跨月记录"), QVariant::fromValue(static_cast<int>(ScopeFilter::CrossMonth)));
    m_scopeFilterCombo->addItem(QStringLiteral(u"跨年记录"), QVariant::fromValue(static_cast<int>(ScopeFilter::CrossYear)));
    m_scopeFilterCombo->setCurrentIndex(0);

    m_addButton = new ElaPushButton(QStringLiteral(u"新增"), this);
    m_editButton = new ElaPushButton(QStringLiteral(u"编辑"), this);
    m_deleteButton = new ElaPushButton(QStringLiteral(u"删除"), this);
    m_reloadButton = new ElaPushButton(QStringLiteral(u"重新加载"), this);
    m_saveButton = new ElaPushButton(QStringLiteral(u"保存变更"), this);
    m_generateButton = new ElaPushButton(QStringLiteral(u"随机生成"), this);

    layout->addWidget(m_searchEdit, 1);
    layout->addWidget(m_scopeFilterCombo);
    layout->addSpacing(12);
    layout->addWidget(m_addButton);
    layout->addWidget(m_editButton);
    layout->addWidget(m_deleteButton);
    layout->addSpacing(12);
    layout->addWidget(m_generateButton);
    layout->addWidget(m_reloadButton);
    layout->addWidget(m_saveButton);

    bodyLayout()->addWidget(toolbar);

    connect(m_searchEdit, &ElaLineEdit::textChanged, this, &SessionsPage::applyFilter);
    connect(m_scopeFilterCombo, &ElaComboBox::currentIndexChanged, this, [this]
            { applyFilter(m_searchEdit->text()); });

    connect(m_addButton, &ElaPushButton::clicked, this, [this]
            { emit requestCreateSession(); });
    connect(m_editButton, &ElaPushButton::clicked, this, [this]
            {
                const auto sessions = selectedSessions();
                if (!sessions.isEmpty())
                    emit requestEditSession(sessions.first()); });
    connect(m_deleteButton, &ElaPushButton::clicked, this, [this]
            {
                const auto sessions = selectedSessions();
                if (!sessions.isEmpty())
                    emit requestDeleteSessions(sessions); });
    connect(m_reloadButton, &ElaPushButton::clicked, this, &SessionsPage::requestReloadSessions);
    connect(m_saveButton, &ElaPushButton::clicked, this, &SessionsPage::requestSaveSessions);
    connect(m_generateButton, &ElaPushButton::clicked, this, &SessionsPage::requestGenerateRandomSessions);
}

void SessionsPage::setupTable()
{
    m_model = std::make_unique<QStandardItemModel>(0, 5, this);
    m_model->setHeaderData(0, Qt::Horizontal, QStringLiteral(u"账号"));
    m_model->setHeaderData(1, Qt::Horizontal, QStringLiteral(u"姓名"));
    m_model->setHeaderData(2, Qt::Horizontal, QStringLiteral(u"开始时间"));
    m_model->setHeaderData(3, Qt::Horizontal, QStringLiteral(u"结束时间"));
    m_model->setHeaderData(4, Qt::Horizontal, QStringLiteral(u"时长(分钟)"));

    m_proxyModel = std::unique_ptr<QSortFilterProxyModel>(new SessionsFilterProxyModel(this));
    m_proxyModel->setSourceModel(m_model.get());

    m_table = new ElaTableView(this);
    m_table->setModel(m_proxyModel.get());
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    auto *header = m_table->horizontalHeader();
    header->setSectionResizeMode(QHeaderView::Fixed);
    header->setStretchLastSection(false);
    m_table->setAlternatingRowColors(true);
    enableAutoFitScaling(m_table);

    bodyLayout()->addWidget(m_table, 1);
}

void SessionsPage::setSessions(const std::vector<Session> &sessions, const QHash<QString, QString> &accountNames)
{
    m_accountNames = accountNames;

    std::vector<Session> filtered;
    filtered.reserve(sessions.size());
    if (m_restrictedAccount.isEmpty())
    {
        filtered = sessions;
    }
    else
    {
        std::copy_if(sessions.begin(), sessions.end(), std::back_inserter(filtered), [&](const Session &s)
                     { return s.account.compare(m_restrictedAccount, Qt::CaseInsensitive) == 0; });
    }

    m_model->removeRows(0, m_model->rowCount());
    m_model->setRowCount(static_cast<int>(filtered.size()));

    for (int row = 0; row < static_cast<int>(filtered.size()); ++row)
    {
        const auto &session = filtered[static_cast<std::size_t>(row)];
        const QString name = accountNames.value(session.account, QString());
        const QString beginText = session.begin.toString(QStringLiteral("yyyy-MM-dd HH:mm"));
        const QString endText = session.end.toString(QStringLiteral("yyyy-MM-dd HH:mm"));
        const int minutes = std::max<int>(0, static_cast<int>((session.begin.secsTo(session.end) + 59) / 60));

        auto *accountItem = new QStandardItem(session.account);
        accountItem->setEditable(false);
        accountItem->setData(session.account, AccountRole);
        accountItem->setData(name, NameRole);
        m_model->setItem(row, 0, accountItem);

        auto *nameItem = new QStandardItem(name);
        nameItem->setEditable(false);
        m_model->setItem(row, 1, nameItem);

        auto *beginItem = new QStandardItem(beginText);
        beginItem->setEditable(false);
        beginItem->setData(session.begin, BeginRole);
        m_model->setItem(row, 2, beginItem);

        auto *endItem = new QStandardItem(endText);
        endItem->setEditable(false);
        endItem->setData(session.end, EndRole);
        m_model->setItem(row, 3, endItem);

        auto *durationItem = new QStandardItem(QString::number(minutes));
        durationItem->setEditable(false);
        durationItem->setData(minutes, MinutesRole);
        m_model->setItem(row, 4, durationItem);
    }

    if (m_table)
        resizeTableToFit(m_table);
}

void SessionsPage::reloadPageData()
{
    if (!m_table)
        return;

    QTimer::singleShot(0, this, [this]
                       { resizeTableToFit(m_table); });
}

void SessionsPage::setAdminMode(bool adminMode)
{
    m_adminMode = adminMode;
    m_addButton->setVisible(m_adminMode);
    m_editButton->setVisible(m_adminMode);
    m_deleteButton->setVisible(m_adminMode);
    m_generateButton->setVisible(m_adminMode);
    m_reloadButton->setVisible(m_adminMode);
    m_saveButton->setVisible(m_adminMode);
    m_table->setSelectionMode(m_adminMode ? QAbstractItemView::ExtendedSelection : QAbstractItemView::NoSelection);
}

void SessionsPage::setRestrictedAccount(const QString &account)
{
    m_restrictedAccount = account;
    if (m_restrictedAccount.isEmpty())
    {
        m_searchEdit->setPlaceholderText(QStringLiteral(u"搜索账号、姓名或时间…"));
    }
    else
    {
        m_searchEdit->setPlaceholderText(QStringLiteral(u"搜索我的上网记录…"));
    }
}

QList<Session> SessionsPage::selectedSessions() const
{
    QList<Session> result;
    if (!m_table->selectionModel())
        return result;

    const auto indexes = m_table->selectionModel()->selectedRows();
    for (const auto &proxyIndex : indexes)
    {
        const auto sourceIndex = m_proxyModel->mapToSource(proxyIndex);
        const auto account = m_model->item(sourceIndex.row(), 0)->data(AccountRole).toString();
        const auto begin = m_model->item(sourceIndex.row(), 2)->data(BeginRole).toDateTime();
        const auto end = m_model->item(sourceIndex.row(), 3)->data(EndRole).toDateTime();
        result.append(Session{account, begin, end});
    }
    return result;
}

void SessionsPage::clearSelection()
{
    if (m_table->selectionModel())
        m_table->selectionModel()->clear();
}

void SessionsPage::applyFilter(const QString &text)
{
    if (!m_proxyModel)
        return;
    auto *proxy = static_cast<SessionsFilterProxyModel *>(m_proxyModel.get());
    proxy->setSearchText(text);
    const auto scope = static_cast<ScopeFilter>(m_scopeFilterCombo->currentData().toInt());
    proxy->setScopeFilter(scope);
}
