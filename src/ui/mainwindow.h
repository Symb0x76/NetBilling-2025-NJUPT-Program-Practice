#pragma once

#include "ElaWindow.h"
#include "backend/models.h"
#include "backend/settings_manager.h"

#include <QList>
#include <QPointer>
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
class SettingsPage;
class ElaToolButton;
class QEvent;
class QPixmap;

class MainWindow : public ElaWindow
{
    Q_OBJECT

public:
    MainWindow(const User &currentUser, QString dataDir, QString outputDir, QWidget *parent = nullptr);
    ~MainWindow() override;

private:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void setupUi();
    void setupNavigation();
    void setupForRole();
    void connectSignals();
    void hideNavigationSearchBox();
    void setupPreferences();
    void applyThemeMode(ElaThemeType::ThemeMode mode);
    void applyAcrylic(bool enabled);
    void updateSettingsPageTheme(ElaThemeType::ThemeMode mode);
    void updateSettingsPageAcrylic(bool enabled);
    void updateSettingsPageAvatar(const QPixmap &pixmap);
    void persistUiSettings();
    void applyUserAvatar();
    QString avatarFilePath() const;
    bool storeAvatarImage(const QString &sourcePath);
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
    void handleChangeAvatar();

    void handleComputeBilling();
    void handleExportBilling();
    void handleRecharge(const QString &account, double amount, const QString &note, bool selfService);

    QString defaultOutputDir() const;
    void ensureOutputDir();
    bool persistUsers();
    bool persistSessions();

    User m_currentUser;
    bool m_isAdmin{false};

    std::unique_ptr<DashboardPage> m_dashboardPage;
    std::unique_ptr<UsersPage> m_usersPage;
    std::unique_ptr<SessionsPage> m_sessionsPage;
    std::unique_ptr<BillingPage> m_billingPage;
    std::unique_ptr<ReportsPage> m_reportsPage;
    std::unique_ptr<RechargePage> m_rechargePage;
    std::unique_ptr<SettingsPage> m_settingsPage;

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
    UiSettings m_uiSettings;
    QPointer<class ElaToolButton> m_navigationSearchButton;
    QPointer<class ElaToolButton> m_navigationToggleButton;
};
