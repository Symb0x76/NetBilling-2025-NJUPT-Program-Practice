#pragma once

#include "ui/pages/BasePage.h"
#include "ElaDef.h"

#include <QPair>
#include <QVector>
#include <QString>

class QChartView;
class QLineSeries;
class QValueAxis;
class QCategoryAxis;
class ElaText;

class UserStatisticsPage : public BasePage
{
    Q_OBJECT

public:
    explicit UserStatisticsPage(QWidget *parent = nullptr);

    void setTrend(const QString &account, const QVector<QPair<QString, double>> &monthlyAmounts);

private:
    void setupContent();
    void updateVisibility();
    void applyTheme(ElaThemeType::ThemeMode mode);

    ElaText *m_intro{nullptr};
    ElaText *m_placeholder{nullptr};
    QChartView *m_trendChartView{nullptr};
    QLineSeries *m_trendSeries{nullptr};
    QValueAxis *m_trendValueAxis{nullptr};
    QCategoryAxis *m_trendCategoryAxis{nullptr};
    QString m_currentAccount;
    bool m_hasTrendData{false};
};
