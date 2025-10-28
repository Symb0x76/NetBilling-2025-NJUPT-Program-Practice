#pragma once

#include "ui/pages/base_page.h"

#include <memory>
#include <QStandardItemModel>
#include <QString>
#include <vector>

class ElaTableView;
class ElaPushButton;
class ElaSpinBox;
class ElaLineEdit;
class ElaText;
struct BillLine;

class BillingPage : public BasePage
{
    Q_OBJECT

public:
    explicit BillingPage(QWidget* parent = nullptr);

    void setBillLines(const std::vector<BillLine>& lines);
    void setSummary(int totalMinutes, double totalAmount, int userCount);
    void setOutputDirectory(const QString& path);
    int selectedYear() const;
    int selectedMonth() const;
    QString outputDirectory() const;

Q_SIGNALS:
    void requestCompute();
    void requestExport();
    void requestBrowseOutputDir();

private:
    void setupToolbar();
    void setupTable();
    void updateSummaryLabel();

    ElaSpinBox* m_yearSpin{nullptr};
    ElaSpinBox* m_monthSpin{nullptr};
    ElaLineEdit* m_outputEdit{nullptr};
    ElaPushButton* m_browseButton{nullptr};
    ElaPushButton* m_computeButton{nullptr};
    ElaPushButton* m_exportButton{nullptr};
    ElaTableView* m_table{nullptr};
    ElaText* m_summaryLabel{nullptr};
    std::unique_ptr<QStandardItemModel> m_model;
    QString m_summaryText;
};
