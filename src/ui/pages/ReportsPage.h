#pragma once

#include "ui/pages/BasePage.h"

#include <memory>
#include <QStandardItemModel>
#include <QVector>

class ElaTableView;
class ElaText;

class ReportsPage : public BasePage
{
    Q_OBJECT

public:
    explicit ReportsPage(QWidget *parent = nullptr);

    void setMonthlySummary(int year, int month, const QVector<int> &usageBuckets, double totalAmount, bool hasBillingData);

private:
    void setupContent();

    ElaTableView *m_table{nullptr};
    std::unique_ptr<QStandardItemModel> m_model;
    ElaText *m_summary{nullptr};
    ElaText *m_placeholder{nullptr};
};
