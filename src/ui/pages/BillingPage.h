#pragma once

#include "ui/pages/BasePage.h"

#include <memory>
#include <QDate>
#include <QPair>
#include <QStandardItemModel>
#include <QString>
#include <QVector>
#include <QtCharts/QChartGlobal>
#include <QtCharts/QCategoryAxis>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <vector>

class ElaTableView;
class ElaPushButton;
class ElaLineEdit;
class ElaText;
class ElaComboBox;
struct BillLine;

class BillingPage : public BasePage
{
    Q_OBJECT

public:
    explicit BillingPage(QWidget *parent = nullptr);

    void setBillLines(const std::vector<BillLine> &lines);
    void setSummary(int totalMinutes, double totalAmount, int userCount);
    void setPersonalTrend(const QString &account, const QVector<QPair<QString, double>> &monthlyAmounts);
    void setOutputDirectory(const QString &path);
    int selectedYear() const;
    int selectedMonth() const;
    QString outputDirectory() const;
    void setUserMode(bool userMode);
    bool isUserMode() const { return m_userMode; }

Q_SIGNALS:
    void requestCompute();
    void requestExport();
    void requestBrowseOutputDir();

private:
    void setupToolbar();
    void setupTable();
    void updateSummaryLabel();
    void reloadPageData() override;
    void syncSelectedMonth(const QDate &date);
    void handleYearMonthChanged();
    void populateYearCombo(ElaComboBox *combo) const;
    void populateMonthCombo(ElaComboBox *combo) const;
    int ensureYearOption(ElaComboBox *combo, int year) const;

    ElaComboBox *m_yearCombo{nullptr};
    ElaComboBox *m_monthCombo{nullptr};
    ElaText *m_selectedMonthLabel{nullptr};
    ElaLineEdit *m_outputEdit{nullptr};
    ElaPushButton *m_browseButton{nullptr};
    ElaPushButton *m_computeButton{nullptr};
    ElaPushButton *m_exportButton{nullptr};
    ElaTableView *m_table{nullptr};
    ElaText *m_summaryLabel{nullptr};
    QChartView *m_trendChartView{nullptr};
    QLineSeries *m_trendSeries{nullptr};
    QValueAxis *m_trendValueAxis{nullptr};
    QCategoryAxis *m_trendCategoryAxis{nullptr};
    bool m_hasTrendData{false};
    std::unique_ptr<QStandardItemModel> m_model;
    QString m_summaryText;
    QWidget *m_toolbar{nullptr};
    QWidget *m_outputRow{nullptr};
    bool m_userMode{false};
    QDate m_selectedMonth;
};
