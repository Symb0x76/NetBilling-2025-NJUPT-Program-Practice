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
#include <QTimer>
#include <QVBoxLayout>
#include <QOpenGLContext>
#include <QSurfaceFormat>
#include <algorithm>
#include <limits>
#include <QtCharts/QCategoryAxis>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

namespace
{
    bool openGLAvailable()
    {
        static bool checked = false;
        static bool available = false;
        if (!checked)
        {
            QOpenGLContext context;
            context.setShareContext(QOpenGLContext::globalShareContext());
            context.setFormat(QSurfaceFormat::defaultFormat());
            available = context.create();
            checked = true;
        }
        return available;
    }

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

    m_table = new ElaTableView(this);
    m_table->setModel(m_model.get());
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    auto *header = m_table->horizontalHeader();
    header->setSectionResizeMode(QHeaderView::Fixed);
    header->setStretchLastSection(false);
    m_table->setAlternatingRowColors(true);
    enableAutoFitScaling(m_table);

    m_summaryLabel = new ElaText(this);
    m_summaryLabel->setTextPixelSize(13);
    m_summaryLabel->setWordWrap(true);
    bodyLayout()->addWidget(m_summaryLabel);

    m_trendSeries = new QLineSeries(this);
    if (openGLAvailable())
        m_trendSeries->setUseOpenGL(true);
    auto *chart = new QChart();
    chart->setAnimationOptions(QChart::SeriesAnimations);
    chart->legend()->hide();
    chart->addSeries(m_trendSeries);

    m_trendValueAxis = new QValueAxis(chart);
    m_trendValueAxis->setLabelFormat("%.2f");
    m_trendValueAxis->setTitleText(QStringLiteral(u"金额 (元)"));
    chart->addAxis(m_trendValueAxis, Qt::AlignLeft);
    m_trendSeries->attachAxis(m_trendValueAxis);

    m_trendCategoryAxis = new QCategoryAxis(chart);
    m_trendCategoryAxis->setLabelsAngle(-45);
    chart->addAxis(m_trendCategoryAxis, Qt::AlignBottom);
    m_trendSeries->attachAxis(m_trendCategoryAxis);

    chart->setTitle(QStringLiteral(u"个人消费趋势"));
    m_trendChartView = new QChartView(chart, this);
    m_trendChartView->setRenderHint(QPainter::Antialiasing);
    m_trendChartView->setMinimumHeight(260);
    m_trendChartView->setVisible(false);
    bodyLayout()->addWidget(m_trendChartView);

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

        m_model->setItem(row, 0, new QStandardItem(line.account));
        m_model->item(row, 0)->setEditable(false);

        m_model->setItem(row, 1, new QStandardItem(line.name));
        m_model->item(row, 1)->setEditable(false);

        const QString planText = planToString(line.plan);
        m_model->setItem(row, 2, new QStandardItem(planText));
        m_model->item(row, 2)->setEditable(false);

        m_model->setItem(row, 3, new QStandardItem(QString::number(line.minutes)));
        m_model->item(row, 3)->setEditable(false);

        m_model->setItem(row, 4, new QStandardItem(locale.toString(line.amount, 'f', 2)));
        m_model->item(row, 4)->setEditable(false);
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

int BillingPage::selectedYear() const
{
    if (m_selectedMonth.isValid())
        return m_selectedMonth.year();
    const QDate today = QDate::currentDate();
    return today.year();
}

int BillingPage::selectedMonth() const
{
    if (m_selectedMonth.isValid())
        return m_selectedMonth.month();
    const QDate today = QDate::currentDate();
    return today.month();
}

QString BillingPage::outputDirectory() const
{
    return m_outputEdit->text().trimmed();
}

void BillingPage::setUserMode(bool userMode)
{
    m_userMode = userMode;
    if (m_toolbar)
        m_toolbar->setVisible(!m_userMode);
    if (m_selectedMonthLabel)
        m_selectedMonthLabel->setVisible(!m_userMode);
    if (m_outputRow)
        m_outputRow->setVisible(!m_userMode);
    if (m_trendChartView)
        m_trendChartView->setVisible(m_userMode && m_hasTrendData);
    if (m_userMode)
    {
        m_table->setSelectionMode(QAbstractItemView::NoSelection);
        m_outputEdit->setVisible(false);
        m_browseButton->setVisible(false);
        m_computeButton->setVisible(false);
        m_exportButton->setVisible(false);
    }
    else
    {
        m_outputEdit->setVisible(true);
        m_browseButton->setVisible(true);
        m_computeButton->setVisible(true);
        m_exportButton->setVisible(true);
    }
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

void BillingPage::setPersonalTrend(const QString &account, const QVector<QPair<QString, double>> &monthlyAmounts)
{
    m_hasTrendData = !account.isEmpty() && !monthlyAmounts.isEmpty();
    if (!m_trendChartView || !m_trendSeries || !m_trendValueAxis || !m_trendCategoryAxis)
        return;

    m_trendSeries->clear();
    const auto labels = m_trendCategoryAxis->categoriesLabels();
    for (const QString &label : labels)
        m_trendCategoryAxis->remove(label);

    if (auto *chart = m_trendChartView->chart())
    {
        if (m_hasTrendData)
            chart->setTitle(QStringLiteral(u"账号 %1 的消费趋势").arg(account));
        else
            chart->setTitle(QStringLiteral(u"个人消费趋势"));
    }

    if (!m_hasTrendData)
    {
        m_trendChartView->setVisible(false);
        return;
    }

    double maxValue = std::numeric_limits<double>::lowest();
    double minValue = std::numeric_limits<double>::max();

    for (int i = 0; i < monthlyAmounts.size(); ++i)
    {
        const double amount = monthlyAmounts.at(i).second;
        m_trendSeries->append(i, amount);
        m_trendCategoryAxis->append(monthlyAmounts.at(i).first, i);
        maxValue = std::max(maxValue, amount);
        minValue = std::min(minValue, amount);
    }

    double upper = (maxValue <= 0.0) ? 1.0 : maxValue * 1.2;
    double lower = (minValue >= 0.0) ? 0.0 : minValue * 1.2;
    if (upper - lower < 0.01)
        upper = lower + 1.0;
    m_trendValueAxis->setRange(lower, upper);

    const qreal upperX = monthlyAmounts.size() > 1 ? static_cast<qreal>(monthlyAmounts.size() - 1) : 0.0;
    m_trendCategoryAxis->setRange(0.0, upperX);

    m_trendChartView->setVisible(m_userMode && m_hasTrendData);
}
