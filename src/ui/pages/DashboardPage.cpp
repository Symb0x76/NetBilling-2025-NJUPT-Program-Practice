#include "ui/pages/DashboardPage.h"

#include "ElaText.h"
#include "backend/Models.h"

#include <QVBoxLayout>

DashboardPage::DashboardPage(QWidget *parent)
    : BasePage(QStringLiteral(u"仪表盘"),
               QStringLiteral(u"快速了解系统运行概况、账户余额及最新账单提示。"),
               parent)
{
    m_welcomeText = new ElaText(this);
    m_welcomeText->setTextPixelSize(16);
    m_welcomeText->setWordWrap(true);

    m_summaryText = new ElaText(this);
    m_summaryText->setTextPixelSize(14);
    m_summaryText->setWordWrap(true);

    m_hintText = new ElaText(this);
    m_hintText->setTextPixelSize(13);
    m_hintText->setWordWrap(true);

    bodyLayout()->addWidget(m_welcomeText);
    bodyLayout()->addWidget(m_summaryText);
    bodyLayout()->addWidget(m_hintText);
    bodyLayout()->addStretch();
}

void DashboardPage::setAdminMode(bool admin)
{
    m_adminMode = admin;
}

void DashboardPage::updateOverview(const User &currentUser,
                                   int totalUsers,
                                   int totalSessions,
                                   double totalMinutes,
                                   double totalAmount,
                                   double balance,
                                   const QString &lastBillingInfo)
{
    const auto buildIdentity = [&currentUser]()
    {
        const QString account = currentUser.account.trimmed();
        const QString name = currentUser.name.trimmed();
        if (account.isEmpty() && name.isEmpty())
            return QString();
        if (name.isEmpty() || name.compare(account, Qt::CaseInsensitive) == 0)
            return account.isEmpty() ? name : account;
        if (account.isEmpty())
            return name;
        return QStringLiteral(u"%1 · %2").arg(account, name);
    };

    const QString identityText = buildIdentity();
    const QString roleText = (currentUser.role == UserRole::Admin) ? QStringLiteral(u"管理员") : QStringLiteral(u"普通用户");
    m_welcomeText->setText(QStringLiteral(u"欢迎使用，%1 (%2)").arg(identityText.isEmpty() ? QStringLiteral(u"用户") : identityText, roleText));

    if (m_adminMode)
    {
        m_summaryText->setText(QStringLiteral(
                                   u"当前共有 %1 位用户，累计上网记录 %2 条，统计总时长约 %3 分钟。最近一次账单金额累计为 %4 元。")
                                   .arg(totalUsers)
                                   .arg(totalSessions)
                                   .arg(static_cast<long long>(totalMinutes))
                                   .arg(QString::number(totalAmount, 'f', 2)));
        m_hintText->setText(lastBillingInfo.isEmpty()
                                ? QStringLiteral(u"还未生成账单，建议及时结算。")
                                : QStringLiteral(u"%1\n账户余额提醒: %2 元").arg(lastBillingInfo, QString::number(balance, 'f', 2)));
    }
    else
    {
        m_summaryText->setText(QStringLiteral(u"当前余额：%1 元。累计上网时长约 %2 分钟。")
                                   .arg(QString::number(balance, 'f', 2))
                                   .arg(static_cast<long long>(totalMinutes)));
        if (balance < 0)
            m_hintText->setText(QStringLiteral(u"您的余额已为负，请尽快充值以免影响上网服务。"));
        else if (balance < 20)
            m_hintText->setText(QStringLiteral(u"余额即将不足 (%1 元)，建议及时充值。").arg(QString::number(balance, 'f', 2)));
        else
            m_hintText->setText(lastBillingInfo);
    }
}
