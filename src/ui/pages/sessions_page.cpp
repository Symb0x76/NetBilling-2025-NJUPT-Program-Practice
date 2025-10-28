#include "ui/pages/sessions_page.h"

#include "ElaComboBox.h"
#include "ElaLineEdit.h"
#include "ElaPushButton.h"
#include "ElaTableView.h"
#include "backend/models.h"

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
}

class SessionsFilterProxyModel : public QSortFilterProxyModel
{
public:
    explicit SessionsFilterProxyModel(QObject* parent = nullptr)
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
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override
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

SessionsPage::SessionsPage(QWidget* parent)
    : BasePage(QStringLiteral(u8"上网记录"),
               QStringLiteral(u8"维护详细的上网起止时间并支持过滤、批量操作与随机数据生成。"),
               parent)
{
    setupToolbar();
    setupTable();
}

void SessionsPage::setupToolbar()
{
    auto* toolbar = new QWidget(this);
    auto* layout = new QHBoxLayout(toolbar);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    m_searchEdit = new ElaLineEdit(this);
    m_searchEdit->setPlaceholderText(QStringLiteral(u8"搜索账号、姓名或时间…"));
    m_searchEdit->setClearButtonEnabled(true);

    m_scopeFilterCombo = new ElaComboBox(this);
    m_scopeFilterCombo->addItem(QStringLiteral(u8"全部记录"), QVariant::fromValue(static_cast<int>(ScopeFilter::All)));
    m_scopeFilterCombo->addItem(QStringLiteral(u8"跨月记录"), QVariant::fromValue(static_cast<int>(ScopeFilter::CrossMonth)));
    m_scopeFilterCombo->addItem(QStringLiteral(u8"跨年记录"), QVariant::fromValue(static_cast<int>(ScopeFilter::CrossYear)));
    m_scopeFilterCombo->setCurrentIndex(0);

    m_addButton = new ElaPushButton(QStringLiteral(u8"新增"), this);
    m_editButton = new ElaPushButton(QStringLiteral(u8"编辑"), this);
    m_deleteButton = new ElaPushButton(QStringLiteral(u8"删除"), this);
    m_reloadButton = new ElaPushButton(QStringLiteral(u8"重新加载"), this);
    m_saveButton = new ElaPushButton(QStringLiteral(u8"保存变更"), this);
    m_generateButton = new ElaPushButton(QStringLiteral(u8"随机生成"), this);

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
    connect(m_scopeFilterCombo, &ElaComboBox::currentIndexChanged, this, [this] { applyFilter(m_searchEdit->text()); });

    connect(m_addButton, &ElaPushButton::clicked, this, [this] { emit requestCreateSession(); });
    connect(m_editButton, &ElaPushButton::clicked, this, [this] {
        const auto sessions = selectedSessions();
        if (!sessions.isEmpty())
            emit requestEditSession(sessions.first());
    });
    connect(m_deleteButton, &ElaPushButton::clicked, this, [this] {
        const auto sessions = selectedSessions();
        if (!sessions.isEmpty())
            emit requestDeleteSessions(sessions);
    });
    connect(m_reloadButton, &ElaPushButton::clicked, this, &SessionsPage::requestReloadSessions);
    connect(m_saveButton, &ElaPushButton::clicked, this, &SessionsPage::requestSaveSessions);
    connect(m_generateButton, &ElaPushButton::clicked, this, &SessionsPage::requestGenerateRandomSessions);
}

void SessionsPage::setupTable()
{
    m_model = std::make_unique<QStandardItemModel>(0, 5, this);
    m_model->setHeaderData(0, Qt::Horizontal, QStringLiteral(u8"账号"));
    m_model->setHeaderData(1, Qt::Horizontal, QStringLiteral(u8"姓名"));
    m_model->setHeaderData(2, Qt::Horizontal, QStringLiteral(u8"开始时间"));
    m_model->setHeaderData(3, Qt::Horizontal, QStringLiteral(u8"结束时间"));
    m_model->setHeaderData(4, Qt::Horizontal, QStringLiteral(u8"时长 (分钟)"));

    m_proxyModel = std::make_unique<SessionsFilterProxyModel>(this);
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
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setAlternatingRowColors(true);

    bodyLayout()->addWidget(m_table, 1);
}

void SessionsPage::setSessions(const std::vector<Session>& sessions, const QHash<QString, QString>& accountNames)
{
    m_model->removeRows(0, m_model->rowCount());
    m_model->setRowCount(static_cast<int>(sessions.size()));

    for (int row = 0; row < static_cast<int>(sessions.size()); ++row)
    {
        const auto& session = sessions[static_cast<std::size_t>(row)];
        const QString name = accountNames.value(session.account, QString());
        const QString beginText = session.begin.toString(QStringLiteral("yyyy-MM-dd HH:mm"));
        const QString endText = session.end.toString(QStringLiteral("yyyy-MM-dd HH:mm"));
        const int minutes = std::max<int>(0, static_cast<int>((session.begin.secsTo(session.end) + 59) / 60));

        auto* accountItem = new QStandardItem(session.account);
        accountItem->setEditable(false);
        accountItem->setData(session.account, AccountRole);
        accountItem->setData(name, NameRole);
        m_model->setItem(row, 0, accountItem);

        auto* nameItem = new QStandardItem(name);
        nameItem->setEditable(false);
        m_model->setItem(row, 1, nameItem);

        auto* beginItem = new QStandardItem(beginText);
        beginItem->setEditable(false);
        beginItem->setData(session.begin, BeginRole);
        m_model->setItem(row, 2, beginItem);

        auto* endItem = new QStandardItem(endText);
        endItem->setEditable(false);
        endItem->setData(session.end, EndRole);
        m_model->setItem(row, 3, endItem);

        auto* durationItem = new QStandardItem(QString::number(minutes));
        durationItem->setEditable(false);
        durationItem->setData(minutes, MinutesRole);
        m_model->setItem(row, 4, durationItem);
    }
}

QList<Session> SessionsPage::selectedSessions() const
{
    QList<Session> result;
    if (!m_table->selectionModel())
        return result;

    const auto indexes = m_table->selectionModel()->selectedRows();
    for (const auto& proxyIndex : indexes)
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

void SessionsPage::applyFilter(const QString& text)
{
    if (!m_proxyModel)
        return;
    m_proxyModel->setSearchText(text);
    const auto scope = static_cast<ScopeFilter>(m_scopeFilterCombo->currentData().toInt());
    m_proxyModel->setScopeFilter(scope);
}
