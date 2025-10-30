#include "ui/pages/UserStatisticsPage.h"

#include "ElaText.h"

#include <QtCharts/QCategoryAxis>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

#include <QVBoxLayout>
#include <QPainter>
#include <QtGlobal>
#include <QStringList>

#include <algorithm>
#include <limits>

UserStatisticsPage::UserStatisticsPage(QWidget *parent)
    : BasePage(QStringLiteral(u"数据统计"),
               QStringLiteral(u"查看最近几个月的个人账单趋势，便于掌握消费变化。"),
               parent)
{
    setupContent();
    updateVisibility();
}

void UserStatisticsPage::setupContent()
{
    m_intro = new ElaText(this);
    m_intro->setTextPixelSize(14);
    m_intro->setWordWrap(true);
    m_intro->setText(QStringLiteral(u"趋势图展示了最近若干个月的账单金额变化，便于及时了解消费走势。"));

    m_placeholder = new ElaText(this);
    m_placeholder->setTextPixelSize(13);
    m_placeholder->setWordWrap(true);
    m_placeholder->setText(QStringLiteral(u"暂未找到可用的数据，请先生成账单或等待系统自动结算。"));

    auto *chart = new QChart();
    chart->legend()->setVisible(false);
    chart->setTitle(QStringLiteral(u"个人消费趋势"));
    chart->setAnimationOptions(QChart::SeriesAnimations);
    chart->setMargins({12, 12, 12, 12});

    m_trendSeries = new QLineSeries(chart);
    chart->addSeries(m_trendSeries);
    m_trendSeries->setPointsVisible(true);

    m_trendValueAxis = new QValueAxis(chart);
    m_trendValueAxis->setLabelFormat(QStringLiteral("%.2f"));
    m_trendValueAxis->setTitleText(QStringLiteral(u"金额 (元)"));
    m_trendValueAxis->setRange(0.0, 1.0);
    m_trendValueAxis->setTickCount(6);
    chart->addAxis(m_trendValueAxis, Qt::AlignLeft);
    m_trendSeries->attachAxis(m_trendValueAxis);

    m_trendCategoryAxis = new QCategoryAxis(chart);
    m_trendCategoryAxis->setLabelsAngle(-45);
    m_trendCategoryAxis->setLabelsPosition(QCategoryAxis::AxisLabelsPositionOnValue);
    chart->addAxis(m_trendCategoryAxis, Qt::AlignBottom);
    m_trendSeries->attachAxis(m_trendCategoryAxis);

    m_trendChartView = new QChartView(chart, this);
    m_trendChartView->setRenderHint(QPainter::Antialiasing, true);
    m_trendChartView->setMinimumHeight(340);

    bodyLayout()->addWidget(m_intro);
    bodyLayout()->addWidget(m_placeholder);
    bodyLayout()->addWidget(m_trendChartView, 1);
}

void UserStatisticsPage::updateVisibility()
{
    const bool showChart = m_hasTrendData && m_trendChartView;
    if (m_trendChartView)
        m_trendChartView->setVisible(showChart);
    if (m_placeholder)
        m_placeholder->setVisible(!showChart);
}

void UserStatisticsPage::setTrend(const QString &account, const QVector<QPair<QString, double>> &monthlyAmounts)
{
    m_currentAccount = account.trimmed();
    m_hasTrendData = !monthlyAmounts.isEmpty();

    if (!m_trendChartView || !m_trendSeries || !m_trendValueAxis || !m_trendCategoryAxis)
    {
        updateVisibility();
        return;
    }

    m_trendSeries->clear();

    const QStringList labels = m_trendCategoryAxis->categoriesLabels();
    for (const QString &label : labels)
        m_trendCategoryAxis->remove(label);

    if (auto *chart = m_trendChartView->chart())
    {
        if (m_hasTrendData && !m_currentAccount.isEmpty())
            chart->setTitle(QStringLiteral(u"账号 %1 的消费趋势").arg(m_currentAccount));
        else
            chart->setTitle(QStringLiteral(u"个人消费趋势"));
    }

    if (!m_hasTrendData)
    {
        m_trendValueAxis->setRange(0.0, 1.0);
        m_trendCategoryAxis->setRange(0.0, 1.0);
        if (m_intro)
            m_intro->setText(QStringLiteral(u"趋势图展示了最近若干个月的账单金额变化，便于及时了解消费走势。"));
        updateVisibility();
        return;
    }

    if (m_intro)
    {
        const QString displayAccount = m_currentAccount.isEmpty()
                                           ? QStringLiteral(u"当前账户")
                                           : m_currentAccount;
        m_intro->setText(QStringLiteral(u"%1 最近几个月的账单金额趋势如下所示。").arg(displayAccount));
    }

    double maxValue = std::numeric_limits<double>::lowest();
    double minValue = std::numeric_limits<double>::max();

    for (int i = 0; i < monthlyAmounts.size(); ++i)
    {
        const auto &entry = monthlyAmounts.at(i);
        const double amount = entry.second;
        m_trendSeries->append(i, amount);
        m_trendCategoryAxis->append(entry.first, i);
        maxValue = std::max(maxValue, amount);
        minValue = std::min(minValue, amount);
    }

    double upper = (maxValue <= 0.0) ? 1.0 : maxValue * 1.2;
    double lower = (minValue >= 0.0) ? 0.0 : minValue * 0.8;
    if (qFuzzyIsNull(upper - lower))
        upper = lower + 1.0;
    if (upper - lower < 0.01)
        upper = lower + 1.0;

    m_trendValueAxis->setRange(lower, upper);

    const qreal maxX = monthlyAmounts.size() > 1 ? static_cast<qreal>(monthlyAmounts.size() - 1) : 0.0;
    m_trendCategoryAxis->setRange(0.0, maxX);

    updateVisibility();
}
