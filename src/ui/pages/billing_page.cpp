#include "ui/pages/billing_page.h"

#include "ElaLineEdit.h"
#include "ElaPushButton.h"
#include "ElaSpinBox.h"
#include "ElaTableView.h"
#include "ElaText.h"
#include "backend/models.h"

#include <QDate>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QVBoxLayout>

#include <QLocale>

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
}

BillingPage::BillingPage(QWidget* parent)
    : BasePage(QStringLiteral(u"账单结算"),
               QStringLiteral(u"选择统计月份后生成账单，可直接导出到指定目录。"),
               parent)
{
    setupToolbar();
    setupTable();
}

void BillingPage::setupToolbar()
{
    auto* toolbar = new QWidget(this);
    auto* layout = new QHBoxLayout(toolbar);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    m_yearSpin = new ElaSpinBox(this);
    m_yearSpin->setRange(2000, 2100);
    m_yearSpin->setValue(QDate::currentDate().year());

    m_monthSpin = new ElaSpinBox(this);
    m_monthSpin->setRange(1, 12);
    m_monthSpin->setValue(QDate::currentDate().month());

    m_outputEdit = new ElaLineEdit(this);
    m_outputEdit->setPlaceholderText(QStringLiteral(u"账单导出目录"));

    m_browseButton = new ElaPushButton(QStringLiteral(u"浏览…"), this);
    m_computeButton = new ElaPushButton(QStringLiteral(u"生成账单"), this);
    m_exportButton = new ElaPushButton(QStringLiteral(u"导出账单"), this);

    layout->addWidget(new ElaText(QStringLiteral(u"统计时间"), this));
    layout->addWidget(m_yearSpin);
    layout->addWidget(new ElaText(QStringLiteral(u"年"), this));
    layout->addWidget(m_monthSpin);
    layout->addWidget(new ElaText(QStringLiteral(u"月"), this));
    layout->addSpacing(12);
    layout->addWidget(m_outputEdit, 1);
    layout->addWidget(m_browseButton);
    layout->addSpacing(12);
    layout->addWidget(m_computeButton);
    layout->addWidget(m_exportButton);

    bodyLayout()->addWidget(toolbar);

    connect(m_computeButton, &ElaPushButton::clicked, this, &BillingPage::requestCompute);
    connect(m_exportButton, &ElaPushButton::clicked, this, &BillingPage::requestExport);
    connect(m_browseButton, &ElaPushButton::clicked, this, &BillingPage::requestBrowseOutputDir);
}

void BillingPage::setupTable()
{
    m_model = std::make_unique<QStandardItemModel>(0, 5, this);
    m_model->setHeaderData(0, Qt::Horizontal, QStringLiteral(u"账号"));
    m_model->setHeaderData(1, Qt::Horizontal, QStringLiteral(u"姓名"));
    m_model->setHeaderData(2, Qt::Horizontal, QStringLiteral(u"套餐"));
    m_model->setHeaderData(3, Qt::Horizontal, QStringLiteral(u"累计时长 (分钟)"));
    m_model->setHeaderData(4, Qt::Horizontal, QStringLiteral(u"应收金额 (元)"));

    m_table = new ElaTableView(this);
    m_table->setModel(m_model.get());
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_table->setAlternatingRowColors(true);

    bodyLayout()->addWidget(m_table, 1);

    m_summaryLabel = new ElaText(this);
    m_summaryLabel->setTextPixelSize(13);
    m_summaryLabel->setWordWrap(true);
    bodyLayout()->addWidget(m_summaryLabel);

    updateSummaryLabel();
}

void BillingPage::setBillLines(const std::vector<BillLine>& lines)
{
    m_model->removeRows(0, m_model->rowCount());
    m_model->setRowCount(static_cast<int>(lines.size()));

    const QLocale locale(QLocale::Chinese, QLocale::China);
    for (int row = 0; row < static_cast<int>(lines.size()); ++row)
    {
        const auto& line = lines[static_cast<std::size_t>(row)];

        m_model->setItem(row, 0, new QStandardItem(line.account));
        m_model->item(row, 0)->setEditable(false);

        m_model->setItem(row, 1, new QStandardItem(line.name));
        m_model->item(row, 1)->setEditable(false);

        const auto planText = planToString(line.plan);
        m_model->setItem(row, 2, new QStandardItem(planText));
        m_model->item(row, 2)->setEditable(false);

        m_model->setItem(row, 3, new QStandardItem(QString::number(line.minutes)));
        m_model->item(row, 3)->setEditable(false);

        m_model->setItem(row, 4, new QStandardItem(locale.toString(line.amount, 'f', 2)));
        m_model->item(row, 4)->setEditable(false);
    }
}

void BillingPage::setSummary(int totalMinutes, double totalAmount, int userCount)
{
    const QLocale locale(QLocale::Chinese, QLocale::China);
    m_summaryText = QStringLiteral(u"共计 %1 位用户，上网总时长 %2 分钟，预计应收 %3 元。")
                        .arg(userCount)
                        .arg(totalMinutes)
                        .arg(locale.toString(totalAmount, 'f', 2));
    updateSummaryLabel();
}

void BillingPage::setOutputDirectory(const QString& path)
{
    m_outputEdit->setText(path);
}

int BillingPage::selectedYear() const
{
    return m_yearSpin->value();
}

int BillingPage::selectedMonth() const
{
    return m_monthSpin->value();
}

QString BillingPage::outputDirectory() const
{
    return m_outputEdit->text().trimmed();
}

void BillingPage::updateSummaryLabel()
{
    if (!m_summaryLabel)
        return;
    if (m_summaryText.isEmpty())
    {
        m_summaryLabel->setText(QStringLiteral(u"还未生成账单。"));
    }
    else
    {
        m_summaryLabel->setText(m_summaryText);
    }
}
