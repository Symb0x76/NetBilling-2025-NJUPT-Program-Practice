#include "ui/pages/ReportsPage.h"

#include "ElaPushButton.h"
#include "ElaTableView.h"
#include "ElaText.h"
#include "backend/Models.h"
#include "ui/ThemeUtils.h"

#include <QFileDialog>
#include <QFile>
#include <QAbstractItemView>
#include <QHeaderView>
#include <QTableView>
#include <QLocale>
#include <QMessageBox>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include <QTextStream>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStringConverter>
#include <QStringList>
#include <algorithm>
#include <numeric>

namespace
{
    QString usageBucketLabel(int index)
    {
        static const QStringList labels{
            QStringLiteral(u"0~30 小时"),
            QStringLiteral(u"30~60 小时"),
            QStringLiteral(u"60~150 小时"),
            QStringLiteral(u"150 小时以上")};
        return (index >= 0 && index < labels.size()) ? labels.at(index) : QString();
    }

    QString planLabel(int plan)
    {
        switch (static_cast<Tariff>(plan))
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
} // namespace

ReportsPage::ReportsPage(QWidget *parent)
    : BasePage(QStringLiteral(u"分析报表"),
               QStringLiteral(u"对历史账单和使用情况做进一步分析，辅助发现异常与趋势。"),
               parent)
{
    setupContent();
}

void ReportsPage::setupContent()
{
    m_usageModel = std::make_unique<QStandardItemModel>(0, 3, this);
    m_usageModel->setHeaderData(0, Qt::Horizontal, QStringLiteral(u"区间说明"));
    m_usageModel->setHeaderData(1, Qt::Horizontal, QStringLiteral(u"用户数"));
    m_usageModel->setHeaderData(2, Qt::Horizontal, QStringLiteral(u"备注"));

    m_usageProxy = std::make_unique<QSortFilterProxyModel>(this);
    m_usageProxy->setSourceModel(m_usageModel.get());
    m_usageProxy->setSortRole(Qt::UserRole);

    m_planModel = std::make_unique<QStandardItemModel>(0, 3, this);
    m_planModel->setHeaderData(0, Qt::Horizontal, QStringLiteral(u"套餐类型"));
    m_planModel->setHeaderData(1, Qt::Horizontal, QStringLiteral(u"用户数"));
    m_planModel->setHeaderData(2, Qt::Horizontal, QStringLiteral(u"本月应收(元)"));

    m_planProxy = std::make_unique<QSortFilterProxyModel>(this);
    m_planProxy->setSourceModel(m_planModel.get());
    m_planProxy->setSortRole(Qt::UserRole);

    m_financeModel = std::make_unique<QStandardItemModel>(0, 3, this);
    m_financeModel->setHeaderData(0, Qt::Horizontal, QStringLiteral(u"统计指标"));
    m_financeModel->setHeaderData(1, Qt::Horizontal, QStringLiteral(u"金额(元)"));
    m_financeModel->setHeaderData(2, Qt::Horizontal, QStringLiteral(u"备注"));

    m_financeProxy = std::make_unique<QSortFilterProxyModel>(this);
    m_financeProxy->setSourceModel(m_financeModel.get());
    m_financeProxy->setSortRole(Qt::UserRole);

    m_placeholder = new ElaText(this);
    m_placeholder->setTextPixelSize(14);
    m_placeholder->setWordWrap(true);
    m_placeholder->setText(QStringLiteral(u"暂无分析数据，请先在“账单结算”页面生成账单。"));
    bodyLayout()->addWidget(m_placeholder);

    m_summary = new ElaText(this);
    m_summary->setTextPixelSize(14);
    m_summary->setWordWrap(true);
    bodyLayout()->addWidget(m_summary);

    m_usageTable = new ElaTableView(this);
    m_usageTable->setModel(m_usageProxy.get());
    m_usageTable->setSelectionMode(QAbstractItemView::NoSelection);
    m_usageTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_usageTable->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *usageHeader = m_usageTable->horizontalHeader();
    usageHeader->setSectionResizeMode(QHeaderView::Fixed);
    usageHeader->setStretchLastSection(false);
    m_usageTable->setAlternatingRowColors(true);
    enableAutoFitScaling(m_usageTable);
    attachTriStateSorting(m_usageTable, m_usageProxy.get());
    bodyLayout()->addWidget(m_usageTable);

    m_planHeader = new ElaText(this);
    m_planHeader->setTextPixelSize(14);
    m_planHeader->setWordWrap(true);
    m_planHeader->setText(QStringLiteral(u"各套餐用户分布"));
    bodyLayout()->addWidget(m_planHeader);

    m_planTable = new ElaTableView(this);
    m_planTable->setModel(m_planProxy.get());
    m_planTable->setSelectionMode(QAbstractItemView::NoSelection);
    m_planTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_planTable->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *planHeader = m_planTable->horizontalHeader();
    planHeader->setSectionResizeMode(QHeaderView::Fixed);
    planHeader->setStretchLastSection(false);
    m_planTable->setAlternatingRowColors(true);
    enableAutoFitScaling(m_planTable);
    attachTriStateSorting(m_planTable, m_planProxy.get());
    bodyLayout()->addWidget(m_planTable);

    m_financeHeader = new ElaText(this);
    m_financeHeader->setTextPixelSize(14);
    m_financeHeader->setWordWrap(true);
    m_financeHeader->setText(QStringLiteral(u"系统收入概览"));
    bodyLayout()->addWidget(m_financeHeader);

    m_financeTable = new ElaTableView(this);
    m_financeTable->setModel(m_financeProxy.get());
    m_financeTable->setSelectionMode(QAbstractItemView::NoSelection);
    m_financeTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_financeTable->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *financeHeader = m_financeTable->horizontalHeader();
    financeHeader->setSectionResizeMode(QHeaderView::Fixed);
    financeHeader->setStretchLastSection(false);
    m_financeTable->setAlternatingRowColors(true);
    enableAutoFitScaling(m_financeTable);
    attachTriStateSorting(m_financeTable, m_financeProxy.get());
    bodyLayout()->addWidget(m_financeTable);

    m_exportRow = new QWidget(this);
    auto *buttonLayout = new QHBoxLayout(m_exportRow);
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->addStretch();
    m_exportCsv = new ElaPushButton(QStringLiteral(u"导出 CSV"), m_exportRow);
    m_exportTxt = new ElaPushButton(QStringLiteral(u"导出 TXT"), m_exportRow);
    buttonLayout->addWidget(m_exportCsv);
    buttonLayout->addWidget(m_exportTxt);
    bodyLayout()->addWidget(m_exportRow);

    connect(m_exportCsv, &ElaPushButton::clicked, this, [this]
            { exportStatistics(ExportFormat::Csv); });
    connect(m_exportTxt, &ElaPushButton::clicked, this, [this]
            { exportStatistics(ExportFormat::Txt); });

    refreshVisibility();
}

void ReportsPage::setMonthlySummary(int year, int month, const QVector<int> &usageBuckets, double totalAmount, bool hasBillingData)
{
    m_currentYear = year;
    m_currentMonth = month;
    m_usageBuckets = usageBuckets;
    if (m_usageBuckets.size() < 4)
        m_usageBuckets.resize(4);
    m_totalAmount = totalAmount;
    m_hasBillingData = hasBillingData;

    if (!m_usageModel)
        return;

    constexpr int kUsageRows = 4;
    m_usageModel->setRowCount(kUsageRows);

    const auto ensureItem = [this](QStandardItemModel *model, int row, int column) -> QStandardItem *
    {
        if (!model)
            return nullptr;
        if (auto *item = model->item(row, column))
            return item;
        auto *fresh = new QStandardItem();
        model->setItem(row, column, fresh);
        return fresh;
    };

    for (int row = 0; row < kUsageRows; ++row)
    {
        if (auto *labelItem = ensureItem(m_usageModel.get(), row, 0))
        {
            labelItem->setText(usageBucketLabel(row));
            labelItem->setEditable(false);
            labelItem->setData(labelItem->text(), Qt::UserRole);
        }

        const int count = (hasBillingData && row < m_usageBuckets.size()) ? m_usageBuckets.at(row) : 0;
        if (auto *countItem = ensureItem(m_usageModel.get(), row, 1))
        {
            countItem->setText(QString::number(count));
            countItem->setEditable(false);
            countItem->setData(count, Qt::UserRole);
        }

        if (auto *noteItem = ensureItem(m_usageModel.get(), row, 2))
        {
            noteItem->setText(QString());
            noteItem->setEditable(false);
            noteItem->setData(QString(), Qt::UserRole);
        }
    }

    if (m_usageTable)
    {
        QTimer::singleShot(0, this, [this]
                           {
                               if (!m_usageTable)
                                   return;
                               m_usageTable->resizeRowsToContents();
                               resizeTableToFit(m_usageTable);
                               setTableFixedHeight(m_usageTable, 4); });
    }

    if (hasBillingData)
    {
        const QLocale locale(QLocale::Chinese, QLocale::China);
        const int totalUsers = std::accumulate(m_usageBuckets.begin(), m_usageBuckets.end(), 0);
        const QString summaryText = QStringLiteral(u"%1 年 %2 月累计应收 %3 元，覆盖 %4 位用户。")
                                        .arg(year)
                                        .arg(month, 2, 10, QLatin1Char('0'))
                                        .arg(locale.toString(totalAmount, 'f', 2))
                                        .arg(totalUsers);
        if (m_summary)
            m_summary->setText(summaryText);
    }
    else if (m_summary)
    {
        m_summary->clear();
    }

    refreshVisibility();
}

void ReportsPage::setPlanDistribution(const QVector<int> &planCounts, const QVector<double> &planAmounts)
{
    m_planCounts = planCounts;
    m_planAmounts = planAmounts;
    if (m_planCounts.size() < 5)
        m_planCounts.resize(5);
    if (m_planAmounts.size() < 5)
        m_planAmounts.resize(5);
    updatePlanTable();
}

void ReportsPage::setFinancialSummary(double billedAmount, double rechargeIncome, double refundAmount, double netIncome)
{
    m_totalAmount = billedAmount;
    m_rechargeIncome = rechargeIncome;
    m_refundAmount = refundAmount;
    m_netIncome = netIncome;
    updateFinanceTable();
}

void ReportsPage::updatePlanTable()
{
    if (!m_planModel)
        return;

    constexpr int kPlanRows = 6; // 5 套餐 + 合计
    m_planModel->setRowCount(kPlanRows);

    const auto ensureItem = [this](QStandardItemModel *model, int row, int column) -> QStandardItem *
    {
        if (!model)
            return nullptr;
        if (auto *item = model->item(row, column))
            return item;
        auto *fresh = new QStandardItem();
        model->setItem(row, column, fresh);
        return fresh;
    };

    const QLocale locale(QLocale::Chinese, QLocale::China);
    int totalUsers = 0;
    double totalAmount = 0.0;

    for (int plan = 0; plan < 5; ++plan)
    {
        if (auto *labelItem = ensureItem(m_planModel.get(), plan, 0))
        {
            labelItem->setText(planLabel(plan));
            labelItem->setEditable(false);
            labelItem->setData(labelItem->text(), Qt::UserRole);
        }

        const int users = (m_hasBillingData && plan < m_planCounts.size()) ? m_planCounts.at(plan) : 0;
        const double amount = (m_hasBillingData && plan < m_planAmounts.size()) ? m_planAmounts.at(plan) : 0.0;
        totalUsers += users;
        totalAmount += amount;

        if (auto *usersItem = ensureItem(m_planModel.get(), plan, 1))
        {
            usersItem->setText(QString::number(users));
            usersItem->setEditable(false);
            usersItem->setData(users, Qt::UserRole);
        }

        if (auto *amountItem = ensureItem(m_planModel.get(), plan, 2))
        {
            amountItem->setText(locale.toString(amount, 'f', 2));
            amountItem->setEditable(false);
            amountItem->setData(amount, Qt::UserRole);
        }
    }

    const int totalRow = 5;
    if (auto *totalLabel = ensureItem(m_planModel.get(), totalRow, 0))
    {
        totalLabel->setText(QStringLiteral(u"合计"));
        totalLabel->setEditable(false);
        totalLabel->setData(totalLabel->text(), Qt::UserRole);
    }
    if (auto *totalUsersItem = ensureItem(m_planModel.get(), totalRow, 1))
    {
        totalUsersItem->setText(QString::number(totalUsers));
        totalUsersItem->setEditable(false);
        totalUsersItem->setData(totalUsers, Qt::UserRole);
    }
    if (auto *totalAmountItem = ensureItem(m_planModel.get(), totalRow, 2))
    {
        totalAmountItem->setText(locale.toString(totalAmount, 'f', 2));
        totalAmountItem->setEditable(false);
        totalAmountItem->setData(totalAmount, Qt::UserRole);
    }

    if (m_planTable)
    {
        QTimer::singleShot(0, this, [this]
                           {
                               if (!m_planTable)
                                   return;
                               m_planTable->resizeRowsToContents();
                               resizeTableToFit(m_planTable);
                               setTableFixedHeight(m_planTable, 6); });
    }

    refreshVisibility();
}

void ReportsPage::updateFinanceTable()
{
    if (!m_financeModel)
        return;

    constexpr int kFinanceRows = 4;
    m_financeModel->setRowCount(kFinanceRows);

    const auto ensureItem = [this](QStandardItemModel *model, int row, int column) -> QStandardItem *
    {
        if (!model)
            return nullptr;
        if (auto *item = model->item(row, column))
            return item;
        auto *fresh = new QStandardItem();
        model->setItem(row, column, fresh);
        return fresh;
    };

    const QLocale locale(QLocale::Chinese, QLocale::China);
    const QString labels[kFinanceRows] = {
        QStringLiteral(u"本月应收账单"),
        QStringLiteral(u"累计充值入账"),
        QStringLiteral(u"退款/扣费支出"),
        QStringLiteral(u"净入账")};

    const double values[kFinanceRows] = {
        m_hasBillingData ? m_totalAmount : 0.0,
        m_hasBillingData ? m_rechargeIncome : 0.0,
        m_hasBillingData ? m_refundAmount : 0.0,
        m_hasBillingData ? m_netIncome : 0.0};

    const QString notes[kFinanceRows] = {
        QString(),
        QString(),
        (m_hasBillingData && m_refundAmount > 0) ? QStringLiteral(u"含管理员扣费") : QString(),
        QStringLiteral(u"充值减去退款")};

    for (int row = 0; row < kFinanceRows; ++row)
    {
        if (auto *labelItem = ensureItem(m_financeModel.get(), row, 0))
        {
            labelItem->setText(labels[row]);
            labelItem->setEditable(false);
            labelItem->setData(labels[row], Qt::UserRole);
        }
        if (auto *valueItem = ensureItem(m_financeModel.get(), row, 1))
        {
            valueItem->setText(locale.toString(values[row], 'f', 2));
            valueItem->setEditable(false);
            valueItem->setData(values[row], Qt::UserRole);
        }
        if (auto *noteItem = ensureItem(m_financeModel.get(), row, 2))
        {
            noteItem->setText(notes[row]);
            noteItem->setEditable(false);
            noteItem->setData(notes[row], Qt::UserRole);
        }
    }

    if (m_financeTable)
    {
        QTimer::singleShot(0, this, [this]
                           {
                               if (!m_financeTable)
                                   return;
                               m_financeTable->resizeRowsToContents();
                               resizeTableToFit(m_financeTable);
                               setTableFixedHeight(m_financeTable, 4); });
    }

    refreshVisibility();
}

void ReportsPage::refreshVisibility()
{
    const bool hasData = m_hasBillingData;
    if (m_placeholder)
        m_placeholder->setVisible(!hasData);
    if (m_summary)
        m_summary->setVisible(hasData);
    if (m_usageTable)
        m_usageTable->setVisible(hasData);
    if (m_planHeader)
        m_planHeader->setVisible(hasData);
    if (m_planTable)
        m_planTable->setVisible(hasData);
    if (m_financeHeader)
        m_financeHeader->setVisible(hasData);
    if (m_financeTable)
        m_financeTable->setVisible(hasData);
    if (m_exportRow)
        m_exportRow->setVisible(hasData);
    if (m_exportCsv)
        m_exportCsv->setEnabled(hasData);
    if (m_exportTxt)
        m_exportTxt->setEnabled(hasData);
}

void ReportsPage::setTableFixedHeight(QTableView *table, int rowCount) const
{
    if (!table || rowCount <= 0)
        return;

    const int frame = table->frameWidth() * 2;
    const int headerHeight = table->horizontalHeader() ? table->horizontalHeader()->height() : 0;

    int rowHeight = 0;
    if (auto *vh = table->verticalHeader())
    {
        if (table->model() && table->model()->rowCount() > 0)
            rowHeight = vh->sectionSize(0);
        if (rowHeight <= 0)
            rowHeight = vh->defaultSectionSize();
        if (rowHeight <= 0)
            rowHeight = table->fontMetrics().height() + 12;
        vh->setSectionResizeMode(QHeaderView::Fixed);
        vh->setDefaultSectionSize(rowHeight);
    }
    else
    {
        rowHeight = table->fontMetrics().height() + 12;
    }

    const int totalHeight = headerHeight + rowCount * rowHeight + frame;
    table->setMinimumHeight(totalHeight);
    table->setMaximumHeight(totalHeight);
}

void ReportsPage::exportStatistics(ExportFormat format)
{
    if (!m_hasBillingData)
        return;

    const QString suffix = format == ExportFormat::Csv ? QStringLiteral("csv") : QStringLiteral("txt");
    const QString defaultName = QStringLiteral("%1年%2月-统计报表.%3")
                                    .arg(m_currentYear)
                                    .arg(m_currentMonth, 2, 10, QLatin1Char('0'))
                                    .arg(suffix);

    const QString selected = QFileDialog::getSaveFileName(this,
                                                          QStringLiteral(u"导出统计报表"),
                                                          defaultName,
                                                          format == ExportFormat::Csv ? QStringLiteral(u"CSV 文件 (*.csv)")
                                                                                      : QStringLiteral(u"文本文件 (*.txt)"));
    if (selected.isEmpty())
        return;

    QFile file(selected);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QMessageBox::warning(this, windowTitle(), QStringLiteral(u"无法写入文件：%1").arg(selected));
        return;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    const QString newline = QStringLiteral("\r\n");

    const auto sep = (format == ExportFormat::Csv) ? QStringLiteral(",") : QStringLiteral("\t");

    auto writeSection = [&](const QString &title)
    {
        out << title << newline;
    };

    auto writeRow = [&](const QStringList &columns)
    {
        for (int i = 0; i < columns.size(); ++i)
        {
            if (i > 0)
                out << sep;
            out << columns.at(i);
        }
        out << newline;
    };

    const QLocale locale(QLocale::Chinese, QLocale::China);

    writeSection(QStringLiteral(u"月度账单汇总"));
    writeRow({QStringLiteral(u"统计月份"), QStringLiteral(u"用户数"), QStringLiteral(u"应收金额(元)")});
    writeRow({QStringLiteral("%1-%2").arg(m_currentYear).arg(m_currentMonth, 2, 10, QLatin1Char('0')),
              QString::number(std::accumulate(m_usageBuckets.begin(), m_usageBuckets.end(), 0)),
              locale.toString(m_totalAmount, 'f', 2)});
    writeSection(QString());

    writeSection(QStringLiteral(u"上网时长区间分布"));
    writeRow({QStringLiteral(u"区间"), QStringLiteral(u"用户数")});
    for (int i = 0; i < m_usageBuckets.size(); ++i)
    {
        writeRow({usageBucketLabel(i), QString::number(m_usageBuckets.at(i))});
    }
    writeSection(QString());

    writeSection(QStringLiteral(u"各套餐用户分布"));
    writeRow({QStringLiteral(u"套餐"), QStringLiteral(u"用户数"), QStringLiteral(u"应收金额(元)")});
    const int planCount = 5;
    for (int plan = 0; plan < planCount; ++plan)
    {
        const int users = (plan < m_planCounts.size()) ? m_planCounts.at(plan) : 0;
        const double amount = (plan < m_planAmounts.size()) ? m_planAmounts.at(plan) : 0.0;
        writeRow({planLabel(plan), QString::number(users), locale.toString(amount, 'f', 2)});
    }
    writeRow({QStringLiteral(u"合计"),
              QString::number(std::accumulate(m_planCounts.begin(), m_planCounts.end(), 0)),
              locale.toString(std::accumulate(m_planAmounts.begin(), m_planAmounts.end(), 0.0), 'f', 2)});
    writeSection(QString());

    writeSection(QStringLiteral(u"系统收入概览"));
    writeRow({QStringLiteral(u"指标"), QStringLiteral(u"金额(元)")});
    writeRow({QStringLiteral(u"本月应收账单"), locale.toString(m_totalAmount, 'f', 2)});
    writeRow({QStringLiteral(u"累计充值入账"), locale.toString(m_rechargeIncome, 'f', 2)});
    writeRow({QStringLiteral(u"退款/扣费支出"), locale.toString(m_refundAmount, 'f', 2)});
    writeRow({QStringLiteral(u"净入账"), locale.toString(m_netIncome, 'f', 2)});

    file.close();
    QMessageBox::information(this, windowTitle(), QStringLiteral(u"统计报表已导出。"));
}
