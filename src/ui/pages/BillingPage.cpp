#include "ui/pages/BillingPage.h"

#include "ElaComboBox.h"
#include "ElaLineEdit.h"
#include "ElaPushButton.h"
#include "ElaTableView.h"
#include "ElaText.h"
#include "backend/models.h"
#include "ui/ThemeUtils.h"

#include <QComboBox>
#include <QDate>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLocale>
#include <QPainter>
#include <QSignalBlocker>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <limits>

namespace
{
    QString planToString(int plan)
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

BillingPage::BillingPage(QWidget *parent)
    : BasePage(QStringLiteral(u"账单结算"),
               QStringLiteral(u"选择统计年月并生成账单，可导出至指定目录。"),
               parent)
{
    setupToolbar();
    setupTable();
}

void BillingPage::setupToolbar()
{
    m_toolbar = new QWidget(this);
    auto *layout = new QVBoxLayout(m_toolbar);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    auto *controlsLayout = new QHBoxLayout();
    controlsLayout->setContentsMargins(0, 0, 0, 0);
    controlsLayout->setSpacing(12);

    m_outputEdit = new ElaLineEdit(this);
    m_outputEdit->setPlaceholderText(QStringLiteral(u"账单导出目录"));

    m_browseButton = new ElaPushButton(QStringLiteral(u"浏览"), this);
    m_computeButton = new ElaPushButton(QStringLiteral(u"生成账单"), this);
    m_exportButton = new ElaPushButton(QStringLiteral(u"导出账单"), this);

    auto *timeLabel = new ElaText(QStringLiteral(u"统计时间"), m_toolbar);
    controlsLayout->addWidget(timeLabel);

    m_yearCombo = new ElaComboBox(m_toolbar);
    m_yearCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    populateYearCombo(m_yearCombo);
    controlsLayout->addWidget(m_yearCombo);

    m_monthCombo = new ElaComboBox(m_toolbar);
    m_monthCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    populateMonthCombo(m_monthCombo);
    controlsLayout->addWidget(m_monthCombo);

    m_selectedMonthLabel = new ElaText(m_toolbar);
    m_selectedMonthLabel->setTextPixelSize(13);
    m_selectedMonthLabel->setWordWrap(true);
    m_selectedMonthLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    controlsLayout->addWidget(m_selectedMonthLabel, 1);

    controlsLayout->addWidget(m_computeButton);
    controlsLayout->addWidget(m_exportButton);

    layout->addLayout(controlsLayout);

    bodyLayout()->addWidget(m_toolbar);

    m_outputRow = new QWidget(this);
    auto *outputLayout = new QHBoxLayout(m_outputRow);
    outputLayout->setContentsMargins(0, 0, 0, 0);
    outputLayout->setSpacing(12);

    auto *outputLabel = new ElaText(QStringLiteral(u"导出目录"), this);
    outputLayout->addWidget(outputLabel);
    outputLayout->addWidget(m_outputEdit, 1);
    outputLayout->addWidget(m_browseButton);

    bodyLayout()->addWidget(m_outputRow);

    connect(m_computeButton, &ElaPushButton::clicked, this, &BillingPage::requestCompute);
    connect(m_exportButton, &ElaPushButton::clicked, this, &BillingPage::requestExport);
    connect(m_browseButton, &ElaPushButton::clicked, this, &BillingPage::requestBrowseOutputDir);

    const QDate today = QDate::currentDate();
    const QDate firstOfMonth(today.year(), today.month(), 1);

    syncSelectedMonth(firstOfMonth);

    connect(m_yearCombo, &ElaComboBox::currentIndexChanged, this, [this](int)
            { handleYearMonthChanged(); });
    connect(m_monthCombo, &ElaComboBox::currentIndexChanged, this, [this](int)
            { handleYearMonthChanged(); });
}

void BillingPage::setupTable()
{
    m_model = std::make_unique<QStandardItemModel>(0, 5, this);
    m_model->setHeaderData(0, Qt::Horizontal, QStringLiteral(u"账号"));
    m_model->setHeaderData(1, Qt::Horizontal, QStringLiteral(u"姓名"));
    m_model->setHeaderData(2, Qt::Horizontal, QStringLiteral(u"套餐"));
    m_model->setHeaderData(3, Qt::Horizontal, QStringLiteral(u"累计时长(分钟)"));
    m_model->setHeaderData(4, Qt::Horizontal, QStringLiteral(u"应收金额(元)"));

    m_proxyModel = std::make_unique<QSortFilterProxyModel>(this);
    m_proxyModel->setSourceModel(m_model.get());
    m_proxyModel->setSortRole(Qt::UserRole);

    m_table = new ElaTableView(this);
    m_table->setModel(m_proxyModel.get());
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    auto *header = m_table->horizontalHeader();
    header->setSectionResizeMode(QHeaderView::Fixed);
    header->setStretchLastSection(false);
    m_table->setAlternatingRowColors(true);
    enableAutoFitScaling(m_table);
    attachTriStateSorting(m_table, m_proxyModel.get());

    m_summaryLabel = new ElaText(this);
    m_summaryLabel->setTextPixelSize(13);
    m_summaryLabel->setWordWrap(true);
    bodyLayout()->addWidget(m_summaryLabel);

    bodyLayout()->addWidget(m_table, 1);

    updateSummaryLabel();
}

void BillingPage::setBillLines(const std::vector<BillLine> &lines)
{
    m_model->removeRows(0, m_model->rowCount());
    m_model->setRowCount(static_cast<int>(lines.size()));

    const QLocale locale(QLocale::Chinese, QLocale::China);
    for (int row = 0; row < static_cast<int>(lines.size()); ++row)
    {
        const auto &line = lines[static_cast<std::size_t>(row)];

        auto *accountItem = new QStandardItem(line.account);
        accountItem->setEditable(false);
        accountItem->setData(line.account, Qt::UserRole);
        m_model->setItem(row, 0, accountItem);

        auto *nameItem = new QStandardItem(line.name);
        nameItem->setEditable(false);
        nameItem->setData(line.name, Qt::UserRole);
        m_model->setItem(row, 1, nameItem);

        const QString planText = planToString(line.plan);
        auto *planItem = new QStandardItem(planText);
        planItem->setEditable(false);
        planItem->setData(static_cast<int>(line.plan), Qt::UserRole);
        m_model->setItem(row, 2, planItem);

        auto *minutesItem = new QStandardItem(QString::number(line.minutes));
        minutesItem->setEditable(false);
        minutesItem->setData(line.minutes, Qt::UserRole);
        m_model->setItem(row, 3, minutesItem);

        auto *amountItem = new QStandardItem(locale.toString(line.amount, 'f', 2));
        amountItem->setEditable(false);
        amountItem->setData(line.amount, Qt::UserRole);
        m_model->setItem(row, 4, amountItem);
    }

    if (m_table)
        resizeTableToFit(m_table);
}

void BillingPage::reloadPageData()
{
    if (!m_table)
        return;

    QTimer::singleShot(0, this, [this]
                       { resizeTableToFit(m_table); });
}

void BillingPage::syncSelectedMonth(const QDate &date)
{
    if (!date.isValid())
        return;

    const QDate first(date.year(), date.month(), 1);

    m_selectedMonth = first;

    if (m_yearCombo)
    {
        const int index = ensureYearOption(m_yearCombo, first.year());
        const QSignalBlocker blocker(m_yearCombo);
        if (index >= 0)
            m_yearCombo->setCurrentIndex(index);
    }

    if (m_monthCombo)
    {
        const int monthIndex = m_monthCombo->findData(first.month());
        const QSignalBlocker blocker(m_monthCombo);
        if (monthIndex >= 0)
            m_monthCombo->setCurrentIndex(monthIndex);
    }

    if (m_selectedMonthLabel)
    {
        const QString text = QStringLiteral(u"当前统计月份：%1 年 %2 月")
                                 .arg(first.year())
                                 .arg(first.month(), 2, 10, QLatin1Char('0'));
        m_selectedMonthLabel->setText(text);
    }
}

void BillingPage::handleYearMonthChanged()
{
    if (!m_yearCombo || !m_monthCombo)
        return;

    const int year = m_yearCombo->currentData().toInt();
    const int month = m_monthCombo->currentData().toInt();
    if (year <= 0 || month <= 0)
        return;

    syncSelectedMonth(QDate(year, month, 1));
}

void BillingPage::populateYearCombo(ElaComboBox *combo) const
{
    if (!combo)
        return;

    combo->clear();
    constexpr int minYear = 1990;
    constexpr int maxYear = 2100;
    for (int year = minYear; year <= maxYear; ++year)
    {
        combo->addItem(QStringLiteral("%1 年").arg(year), year);
    }
}

void BillingPage::populateMonthCombo(ElaComboBox *combo) const
{
    if (!combo)
        return;

    combo->clear();
    for (int month = 1; month <= 12; ++month)
    {
        combo->addItem(QStringLiteral("%1 月").arg(month), month);
    }
}

int BillingPage::ensureYearOption(ElaComboBox *combo, int year) const
{
    if (!combo)
        return -1;

    int index = combo->findData(year);
    if (index >= 0)
        return index;

    const QSignalBlocker blocker(combo);
    int insertPos = 0;
    for (; insertPos < combo->count(); ++insertPos)
    {
        if (combo->itemData(insertPos).toInt() > year)
            break;
    }
    combo->insertItem(insertPos, QStringLiteral("%1 年").arg(year), year);
    return combo->findData(year);
}

void BillingPage::setSummary(int totalMinutes, double totalAmount, int userCount)
{
    const QLocale locale(QLocale::Chinese, QLocale::China);
    if (m_userMode)
    {
        m_summaryText = QStringLiteral(u"本月上网共计 %1 分钟，应扣费用 %2 元。")
                            .arg(totalMinutes)
                            .arg(locale.toString(totalAmount, 'f', 2));
    }
    else
    {
        m_summaryText = QStringLiteral(u"共 %1 位用户，上网时长累计 %2 分钟，账单金额合计 %3 元。")
                            .arg(userCount)
                            .arg(totalMinutes)
                            .arg(locale.toString(totalAmount, 'f', 2));
    }
    updateSummaryLabel();
}

void BillingPage::setOutputDirectory(const QString &path)
{
    m_outputEdit->setText(path);
}

void BillingPage::updateSummaryLabel()
{
    if (!m_summaryLabel)
        return;
    if (m_summaryText.isEmpty())
    {
        m_summaryLabel->setText(QStringLiteral(u"尚未生成任何账单。"));
    }
    else
    {
        m_summaryLabel->setText(m_summaryText);
    }
}

int BillingPage::selectedYear() const
{
    if (m_yearCombo)
        return m_yearCombo->currentData().toInt();
    return QDate::currentDate().year();
}

int BillingPage::selectedMonth() const
{
    if (m_monthCombo)
        return m_monthCombo->currentData().toInt();
    return QDate::currentDate().month();
}

QString BillingPage::outputDirectory() const
{
    return m_outputEdit ? m_outputEdit->text().trimmed() : QString();
}

void BillingPage::setUserMode(bool userMode)
{
    if (m_userMode == userMode)
        return;

    m_userMode = userMode;

    if (m_computeButton)
    {
        m_computeButton->setVisible(true);
        m_computeButton->setText(m_userMode ? QStringLiteral(u"生成我的账单") : QStringLiteral(u"生成账单"));
    }

    const bool showExportControls = !m_userMode;
    if (m_exportButton)
        m_exportButton->setVisible(showExportControls);
    if (m_browseButton)
        m_browseButton->setVisible(showExportControls);
    if (m_outputRow)
        m_outputRow->setVisible(showExportControls);
    if (m_outputEdit)
    {
        m_outputEdit->setReadOnly(!showExportControls);
        m_outputEdit->setEnabled(showExportControls);
    }

    if (m_yearCombo)
        m_yearCombo->setEnabled(true);
    if (m_monthCombo)
        m_monthCombo->setEnabled(true);

    updateSummaryLabel();
}
