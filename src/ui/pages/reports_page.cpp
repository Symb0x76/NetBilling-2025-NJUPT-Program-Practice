#include "ui/pages/reports_page.h"

#include "ElaTableView.h"
#include "ElaText.h"

#include <QHeaderView>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QVBoxLayout>

#include <QLocale>

ReportsPage::ReportsPage(QWidget *parent)
    : BasePage(QStringLiteral(u"分析报表"),
               QStringLiteral(u"对历史账单和使用情况做进一步分析，辅助发现异常与趋势。"),
               parent)
{
    setupContent();
}

void ReportsPage::setupContent()
{
    m_model = std::make_unique<QStandardItemModel>(0, 3, this);
    m_model->setHeaderData(0, Qt::Horizontal, QStringLiteral(u"区间说明"));
    m_model->setHeaderData(1, Qt::Horizontal, QStringLiteral(u"用户数"));
    m_model->setHeaderData(2, Qt::Horizontal, QStringLiteral(u"备注"));

    m_table = new ElaTableView(this);
    m_table->setModel(m_model.get());
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_table->setAlternatingRowColors(true);

    bodyLayout()->addWidget(m_table);

    m_summary = new ElaText(this);
    m_summary->setTextPixelSize(14);
    m_summary->setWordWrap(true);
    bodyLayout()->addWidget(m_summary);
}

void ReportsPage::setMonthlySummary(int year, int month, const QVector<int> &usageBuckets, double totalAmount)
{
    static const QStringList bucketLabels{
        QStringLiteral(u"0~30 小时"),
        QStringLiteral(u"30~60 小时"),
        QStringLiteral(u"60~150 小时"),
        QStringLiteral(u"150 小时以上")};

    m_model->removeRows(0, m_model->rowCount());
    m_model->setRowCount(bucketLabels.size());

    for (int row = 0; row < bucketLabels.size(); ++row)
    {
        auto *labelItem = new QStandardItem(bucketLabels.at(row));
        labelItem->setEditable(false);
        m_model->setItem(row, 0, labelItem);

        const int count = (row < usageBuckets.size()) ? usageBuckets.at(row) : 0;
        auto *countItem = new QStandardItem(QString::number(count));
        countItem->setEditable(false);
        m_model->setItem(row, 1, countItem);

        auto *remarkItem = new QStandardItem(QString());
        remarkItem->setEditable(false);
        m_model->setItem(row, 2, remarkItem);
    }

    const QLocale locale(QLocale::Chinese, QLocale::China);
    const QString text = QStringLiteral(u"%1 年 %2 月累计应收：%3 元")
                             .arg(year)
                             .arg(month, 2, 10, QLatin1Char('0'))
                             .arg(locale.toString(totalAmount, 'f', 2));
    m_summary->setText(text);
}
