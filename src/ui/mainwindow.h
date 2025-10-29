#pragma once

#include "ElaWindow.h"
#include "backend/models.h"

#include <QList>
#include <QStringList>
#include <QVector>
#include <memory>
#include <vector>

class Repository;
class DashboardPage;
class UsersPage;
class SessionsPage;
class BillingPage;
class ReportsPage;
class RechargePage;

class MainWindow : public ElaWindow
{
    Q_OBJECT

public:
    MainWindow(const User &currentUser, QString dataDir, QString outputDir, QWidget *parent = nullptr);
    ~MainWindow() override;

private:
    void setupUi();
    void setupNavigation();
    void setupForRole();
    void connectSignals();
    void loadInitialData();
    void validateSessions();
    void refreshUsersPage();
    void refreshSessionsPage();
    void refreshBillingSummary();
    void refreshRechargePage();
    void resetComputedBills();

    void handleCreateUser();
    void handleEditUser(const QString &account);
    void handleDeleteUsers(const QStringList &accounts);
    void handleReloadUsers();
    void handleSaveUsers();

    void handleCreateSession();
    void handleEditSession(const Session &session);
    void handleDeleteSessions(const QList<Session> &sessions);
    void handleReloadSessions();
    void handleSaveSessions();
    void handleGenerateRandomSessions();

    void handleComputeBilling();
    void handleExportBilling();
    void handleRecharge(const QString &account, double amount, const QString &note, bool selfService);

    QString defaultOutputDir() const;
    void ensureOutputDir();
    bool persistUsers();
    bool persistSessions();

    User m_currentUser;
    bool m_isAdmin{false};

    QString m_dataGroupKey;
    QString m_reportsGroupKey;

    std::unique_ptr<DashboardPage> m_dashboardPage;
    std::unique_ptr<UsersPage> m_usersPage;
    std::unique_ptr<SessionsPage> m_sessionsPage;
    std::unique_ptr<BillingPage> m_billingPage;
    std::unique_ptr<ReportsPage> m_reportsPage;
    std::unique_ptr<RechargePage> m_rechargePage;

    std::unique_ptr<Repository> m_repository;
    std::vector<User> m_users;
    std::vector<Session> m_sessions;
    std::vector<BillLine> m_latestBills;
    std::vector<RechargeRecord> m_recharges;

    bool m_usersDirty{false};
    bool m_sessionsDirty{false};
    QString m_dataDir;
    QString m_outputDir;
    bool m_hasComputed{false};
    int m_lastBillYear{0};
    int m_lastBillMonth{0};
    double m_currentBalance{0.0};
    int m_currentUserIndex{-1};
    QString m_lastBillingInfo;
    bool m_rechargesDirty{false};
};
