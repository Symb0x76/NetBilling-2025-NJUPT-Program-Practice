#pragma once

#include "ElaWindow.h"
#include "backend/models.h"

#include <QList>
#include <QStringList>
#include <memory>
#include <optional>
#include <QString>
#include <QVector>
#include <vector>

class Repository;
class DashboardPage;
class UsersPage;
class SessionsPage;
class BillingPage;
class ReportsPage;

class MainWindow : public ElaWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private:
    void setupUi();
    void setupNavigation();
    void connectSignals();
    void loadInitialData();
    void refreshUsersPage();
    void refreshSessionsPage();
    void refreshBillingSummary();
    void resetComputedBills();

    void handleCreateUser();
    void handleEditUser(const QString& account);
    void handleDeleteUsers(const QStringList& accounts);
    void handleReloadUsers();
    void handleSaveUsers();

    void handleCreateSession();
    void handleEditSession(const Session& session);
    void handleDeleteSessions(const QList<Session>& sessions);
    void handleReloadSessions();
    void handleSaveSessions();
    void handleGenerateRandomSessions();
    void handleComputeBilling();
    void handleExportBilling();

    QString defaultOutputDir() const;
    void ensureOutputDir();

    QString m_dataGroupKey;
    QString m_reportsGroupKey;

    std::unique_ptr<DashboardPage> m_dashboardPage;
    std::unique_ptr<UsersPage> m_usersPage;
    std::unique_ptr<SessionsPage> m_sessionsPage;
    std::unique_ptr<BillingPage> m_billingPage;
    std::unique_ptr<ReportsPage> m_reportsPage;

    std::unique_ptr<Repository> m_repository;
    std::vector<User> m_users;
    std::vector<Session> m_sessions;
    std::vector<BillLine> m_latestBills;
    bool m_usersDirty{false};
    bool m_sessionsDirty{false};
    QString m_dataDir;
    QString m_outputDir;
    bool m_hasComputed{false};
    int m_lastBillYear{0};
    int m_lastBillMonth{0};
};



