#pragma once

#include "ui/pages/BasePage.h"

#include <memory>
#include <QStandardItemModel>
#include <QVector>

class ElaTableView;
class ElaText;
class ElaPushButton;
class QWidget;
class QTableView;
class QSortFilterProxyModel;

class ReportsPage : public BasePage
{
    Q_OBJECT

public:
    explicit ReportsPage(QWidget *parent = nullptr);

    void setMonthlySummary(int year, int month, const QVector<int> &usageBuckets, double totalAmount, bool hasBillingData);
    void setPlanDistribution(const QVector<int> &planCounts, const QVector<double> &planAmounts);
    void setFinancialSummary(double billedAmount, double rechargeIncome, double refundAmount, double netIncome);

private:
    enum class ExportFormat
    {
        Csv,
        Txt
    };

    void setupContent();
    void updatePlanTable();
    void updateFinanceTable();
    void refreshVisibility();
    void exportStatistics(ExportFormat format);
    void setTableFixedHeight(QTableView *table, int rowCount) const;

    ElaTableView *m_usageTable{nullptr};
    ElaTableView *m_planTable{nullptr};
    ElaTableView *m_financeTable{nullptr};
    std::unique_ptr<QStandardItemModel> m_usageModel;
    std::unique_ptr<QStandardItemModel> m_planModel;
    std::unique_ptr<QStandardItemModel> m_financeModel;
    std::unique_ptr<QSortFilterProxyModel> m_usageProxy;
    std::unique_ptr<QSortFilterProxyModel> m_planProxy;
    std::unique_ptr<QSortFilterProxyModel> m_financeProxy;
    ElaText *m_summary{nullptr};
    ElaText *m_planHeader{nullptr};
    ElaText *m_financeHeader{nullptr};
    ElaText *m_placeholder{nullptr};
    QWidget *m_exportRow{nullptr};
    ElaPushButton *m_exportCsv{nullptr};
    ElaPushButton *m_exportTxt{nullptr};

    QVector<int> m_usageBuckets;
    QVector<int> m_planCounts;
    QVector<double> m_planAmounts;
    double m_totalAmount{0.0};
    double m_rechargeIncome{0.0};
    double m_refundAmount{0.0};
    double m_netIncome{0.0};
    int m_currentYear{0};
    int m_currentMonth{0};
    bool m_hasBillingData{false};
};
