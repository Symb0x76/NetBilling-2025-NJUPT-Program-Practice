#include "ui/pages/dashboard_page.h"

#include "ElaText.h"

#include <QVBoxLayout>

DashboardPage::DashboardPage(QWidget* parent)
    : BasePage(QStringLiteral(u"仪表盘"), QStringLiteral(u"快速浏览系统运行数据、提醒与最近生成的账单。"), parent)
{
    auto* text = new ElaText(QStringLiteral(u"仪表盘内容开发中…"), this);
    text->setTextPixelSize(15);
    bodyLayout()->addWidget(text);
    bodyLayout()->addStretch();
}
