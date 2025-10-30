#pragma once

#include "ui/pages/BasePage.h"
#include "backend/Models.h"
#include <QString>

class ElaText;

class DashboardPage : public BasePage
{
    Q_OBJECT

public:
    explicit DashboardPage(QWidget *parent = nullptr);

    void setAdminMode(bool admin);
    void updateOverview(const User &currentUser,
                        int totalUsers,
                        int totalSessions,
                        double totalMinutes,
                        double totalAmount,
                        double balance,
                        const QString &lastBillingInfo);

private:
    ElaText *m_identityText{nullptr};
    ElaText *m_welcomeText{nullptr};
    ElaText *m_summaryText{nullptr};
    ElaText *m_hintText{nullptr};
    bool m_adminMode{false};
};
