#pragma once

#include "ui/pages/base_page.h"

#include <memory>
#include <QVector>

class ElaTableView;
class QStandardItemModel;
class ElaText;

class ReportsPage : public BasePage
{
    Q_OBJECT

public:
    explicit ReportsPage(QWidget* parent = nullptr);

    void setMonthlySummary(int year, int month, const QVector<int>& usageBuckets, double totalAmount);

private:
    void setupContent();

    ElaTableView* m_table{nullptr};
    std::unique_ptr<QStandardItemModel> m_model;
    ElaText* m_summary{nullptr};
};
