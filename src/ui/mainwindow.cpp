#include "ui/MainWindow.h"

#include "backend/Billing.h"
#include "backend/Repository.h"
#include "backend/Security.h"
#include "backend/SettingsManager.h"
#include "ui/dialogs/LoginDialog.h"
#include "ui/dialogs/PasswordDialog.h"
#include "ui/dialogs/SessionEditorDialog.h"
#include "ui/dialogs/UserEditorDialog.h"
#include "ui/pages/BillingPage.h"
#include "ui/pages/DashboardPage.h"
#include "ui/pages/RechargePage.h"
#include "ui/pages/ReportsPage.h"
#include "ui/pages/SessionsPage.h"
#include "ui/pages/SettingsPage.h"
#include "ui/pages/UsersPage.h"
#include "ui/pages/UserStatisticsPage.h"

#include "ElaApplication.h"
#include "ElaNavigationBar.h"
#include "ElaText.h"
#include "ElaTheme.h"
#include "ElaThemeAnimationWidget.h"

#include <QChar>
#include <QDate>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHash>
#include <QIcon>
#include <QMessageBox>
#include <QMetaType>
#include <QRandomGenerator>
#include <QSet>
#include <QSignalBlocker>
#include <QSize>
#include <QSizePolicy>
#include <QStringConverter>
#include <QStringList>
#include <QTextStream>
#include <QTime>
#include <QTimer>
#include <QVector>
#include <QtMath>
#include <algorithm>
#include <iterator>
#include <numeric>
#include <utility>

namespace
{
    QString accountBannerTextFor(const User &user)
    {
        const QString account = user.account.trimmed();
        const QString name = user.name.trimmed();
        if (account.isEmpty())
            return name;
        if (name.isEmpty() || name.compare(account, Qt::CaseInsensitive) == 0)
            return account;
        return QStringLiteral(u"%1 · %2").arg(account, name);
    }

    bool userLess(const User &a, const User &b)
    {
        return a.account < b.account;
    }

    bool sessionLess(const Session &a, const Session &b)
    {
        if (a.account != b.account)
            return a.account < b.account;
        if (a.begin != b.begin)
            return a.begin < b.begin;
        return a.end < b.end;
    }

    bool sessionsEqual(const Session &lhs, const Session &rhs)
    {
        return lhs.account == rhs.account && lhs.begin == rhs.begin && lhs.end == rhs.end;
    }
} // namespace

MainWindow::MainWindow(const User &currentUser, QString dataDir, QString outputDir, QWidget *parent)
    : ElaWindow(parent), m_currentUser(currentUser), m_isAdmin(currentUser.role == UserRole::Admin), m_dataDir(std::move(dataDir)), m_outputDir(std::move(outputDir))
{
    qRegisterMetaType<Session>("Session");
    qRegisterMetaType<QList<Session>>("QList<Session>");

    m_uiSettings = loadUiSettings(m_dataDir);
    applyThemeMode(m_uiSettings.themeMode);
    applyAcrylic(m_uiSettings.acrylicEnabled);

    setupUi();
    setupNavigation();
    setupPreferences();
    connectSignals();

    ensureOutputDir();
    m_repository = std::make_unique<Repository>(m_dataDir, m_outputDir);

    loadInitialData();
    setupForRole();
    refreshUsersPage();
    refreshSessionsPage();
    refreshBillingSummary();
    refreshRechargePage();
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUi()
{
    setWindowTitle(QStringLiteral(u"上网计费系统"));
    setMinimumSize({1100, 720});
    resize(1280, 820);
    setWindowIcon(QIcon(QStringLiteral(":/icons/flash-timer.svg")));

    setNavigationBarWidth(260);
    setNavigationBarDisplayMode(ElaNavigationType::Maximal);
    setUserInfoCardVisible(false);

    // Account info is now presented inside the dashboard instead of the app bar.

    setWindowButtonFlag(ElaAppBarType::ThemeChangeButtonHint);
}

void MainWindow::setupNavigation()
{
    m_dashboardPage = std::make_unique<DashboardPage>(this);
    addPageNode(QStringLiteral(u"仪表盘"), m_dashboardPage.get(), ElaIconType::GaugeHigh);

    if (m_isAdmin)
    {
        m_usersPage = std::make_unique<UsersPage>(this);
        m_sessionsPage = std::make_unique<SessionsPage>(this);
        m_billingPage = std::make_unique<BillingPage>(this);
        m_rechargePage = std::make_unique<RechargePage>(this);
        m_reportsPage = std::make_unique<ReportsPage>(this);

        addPageNode(QStringLiteral(u"用户管理"), m_usersPage.get(), ElaIconType::PeopleGroup);
        addPageNode(QStringLiteral(u"上网记录"), m_sessionsPage.get(), ElaIconType::Timeline);
        addPageNode(QStringLiteral(u"账单结算"), m_billingPage.get(), ElaIconType::Calculator);
        m_billingPageKey = m_billingPage->property("ElaPageKey").toString();
        addPageNode(QStringLiteral(u"余额充值"), m_rechargePage.get(), ElaIconType::Wallet);
        addPageNode(QStringLiteral(u"统计分析"), m_reportsPage.get(), ElaIconType::ChartColumn);
    }
    else
    {
        m_sessionsPage = std::make_unique<SessionsPage>(this);
        m_billingPage = std::make_unique<BillingPage>(this);
        m_rechargePage = std::make_unique<RechargePage>(this);
        m_userStatsPage = std::make_unique<UserStatisticsPage>(this);

        addPageNode(QStringLiteral(u"上网记录"), m_sessionsPage.get(), ElaIconType::Timeline);
        addPageNode(QStringLiteral(u"账单概览"), m_billingPage.get(), ElaIconType::Calculator);
        m_billingPageKey = m_billingPage->property("ElaPageKey").toString();
        addPageNode(QStringLiteral(u"余额充值"), m_rechargePage.get(), ElaIconType::Wallet);
        addPageNode(QStringLiteral(u"数据统计"), m_userStatsPage.get(), ElaIconType::ChartColumn);
    }

    m_settingsPage = std::make_unique<SettingsPage>(this);
    addPageNode(QStringLiteral(u"系统设置"), m_settingsPage.get(), ElaIconType::Gear);
    m_settingsPage->setDataManagementVisible(m_isAdmin);
    updateSettingsPageTheme(m_uiSettings.themeMode);
    updateSettingsPageAcrylic(m_uiSettings.acrylicEnabled);

    setCurrentStackIndex(0);
}

void MainWindow::setupForRole()
{
    if (m_dashboardPage)
        m_dashboardPage->setAdminMode(m_isAdmin);
    if (m_usersPage)
    {
        m_usersPage->setAdminMode(true);
        m_usersPage->setCurrentAccount(m_currentUser.account);
    }
    if (m_sessionsPage)
    {
        m_sessionsPage->setAdminMode(m_isAdmin);
        m_sessionsPage->setRestrictedAccount(m_isAdmin ? QString() : m_currentUser.account);
    }
    if (m_billingPage)
        m_billingPage->setUserMode(!m_isAdmin);
    if (m_rechargePage)
    {
        m_rechargePage->setAdminMode(m_isAdmin);
        m_rechargePage->setCurrentAccount(m_currentUser.account);
        m_rechargePage->setCurrentBalance(m_currentUser.balance);
    }

    updateAccountBanner();
}

void MainWindow::connectSignals()
{
    if (m_usersPage)
    {
        connect(m_usersPage.get(), &UsersPage::requestCreateUser, this, &MainWindow::handleCreateUser);
        connect(m_usersPage.get(), &UsersPage::requestEditUser, this, &MainWindow::handleEditUser);
        connect(m_usersPage.get(), &UsersPage::requestDeleteUsers, this, &MainWindow::handleDeleteUsers);
        connect(m_usersPage.get(), &UsersPage::requestReloadUsers, this, &MainWindow::handleReloadUsers);
        connect(m_usersPage.get(), &UsersPage::requestSaveUsers, this, &MainWindow::handleSaveUsers);
    }

    if (m_sessionsPage)
    {
        connect(m_sessionsPage.get(), &SessionsPage::requestCreateSession, this, &MainWindow::handleCreateSession);
        connect(m_sessionsPage.get(), &SessionsPage::requestEditSession, this, &MainWindow::handleEditSession);
        connect(m_sessionsPage.get(), &SessionsPage::requestDeleteSessions, this, &MainWindow::handleDeleteSessions);
        connect(m_sessionsPage.get(), &SessionsPage::requestReloadSessions, this, &MainWindow::handleReloadSessions);
        connect(m_sessionsPage.get(), &SessionsPage::requestSaveSessions, this, &MainWindow::handleSaveSessions);
        connect(m_sessionsPage.get(), &SessionsPage::requestGenerateRandomSessions, this, &MainWindow::handleGenerateRandomSessions);
    }

    if (m_billingPage)
    {
        connect(m_billingPage.get(), &BillingPage::requestCompute, this, &MainWindow::handleComputeBilling);
        connect(m_billingPage.get(), &BillingPage::requestExport, this, &MainWindow::handleExportBilling);
        connect(m_billingPage.get(), &BillingPage::requestBrowseOutputDir, this, [this]
                {
            const QString selected = QFileDialog::getExistingDirectory(this, QStringLiteral(u"选择导出目录"), m_outputDir);
            if (selected.isEmpty())
                return;
            m_outputDir = selected;
            ensureOutputDir();
            if (m_billingPage)
                m_billingPage->setOutputDirectory(m_outputDir);
            m_repository = std::make_unique<Repository>(m_dataDir, m_outputDir);
            resetComputedBills(); });
    }

    connect(this, &ElaWindow::pCurrentStackIndexChanged, this, &MainWindow::handleStackIndexChanged);

    if (m_rechargePage)
    {
        connect(m_rechargePage.get(), &RechargePage::rechargeRequested, this, &MainWindow::handleRecharge);
    }

    if (m_settingsPage)
    {
        connect(m_settingsPage.get(), &SettingsPage::darkModeToggled, this, [this](bool enabled)
                { applyThemeMode(enabled ? ElaThemeType::Dark : ElaThemeType::Light); });
        connect(m_settingsPage.get(), &SettingsPage::acrylicToggled, this, [this](bool enabled)
                { applyAcrylic(enabled); });
        connect(m_settingsPage.get(), &SettingsPage::changePasswordRequested, this, &MainWindow::handleChangePasswordRequest);
        connect(m_settingsPage.get(), &SettingsPage::switchAccountRequested, this, &MainWindow::handleSwitchAccountRequested);
        connect(m_settingsPage.get(), &SettingsPage::backupRequested, this, &MainWindow::handleBackupRequested);
        connect(m_settingsPage.get(), &SettingsPage::restoreRequested, this, &MainWindow::handleRestoreRequested);
    }
}

void MainWindow::setupPreferences()
{
    const auto currentTheme = eTheme->getThemeMode();
    updateSettingsPageTheme(currentTheme);
    if (m_uiSettings.themeMode != currentTheme)
    {
        m_uiSettings.themeMode = currentTheme;
        persistUiSettings();
    }

#ifdef Q_OS_WIN
    const bool acrylicActive = eApp->getWindowDisplayMode() == ElaApplicationType::Acrylic;
#else
    const bool acrylicActive = false;
#endif
    setIsCentralStackedWidgetTransparent(acrylicActive);
    updateSettingsPageAcrylic(acrylicActive);
    if (m_uiSettings.acrylicEnabled != acrylicActive)
    {
        m_uiSettings.acrylicEnabled = acrylicActive;
        persistUiSettings();
    }

    connect(eTheme, &ElaTheme::themeModeChanged, this, [this](ElaThemeType::ThemeMode mode)
            {
                updateSettingsPageTheme(mode);
                if (m_uiSettings.themeMode != mode)
                {
                    m_uiSettings.themeMode = mode;
                    persistUiSettings();
                } });

    connect(eApp, &ElaApplication::pWindowDisplayModeChanged, this, [this]()
            {
#ifdef Q_OS_WIN
                const bool acrylicOn = eApp->getWindowDisplayMode() == ElaApplicationType::Acrylic;
#else
                const bool acrylicOn = false;
#endif
                setIsCentralStackedWidgetTransparent(acrylicOn);
                updateSettingsPageAcrylic(acrylicOn);
                if (m_uiSettings.acrylicEnabled != acrylicOn)
                {
                    m_uiSettings.acrylicEnabled = acrylicOn;
                    persistUiSettings();
                } });
}

void MainWindow::applyThemeMode(ElaThemeType::ThemeMode mode)
{
    const auto currentMode = eTheme->getThemeMode();
    if (currentMode == mode)
    {
        updateSettingsPageTheme(mode);
        return;
    }

    ElaThemeAnimationWidget *overlay = nullptr;
    if (isVisible())
    {
        overlay = new ElaThemeAnimationWidget(this);
        overlay->move(0, 0);
        overlay->resize(size());
        overlay->setOldWindowBackground(grab(rect()).toImage());
        overlay->raise();
        overlay->show();
    }

    eTheme->setThemeMode(mode);

    if (overlay)
        overlay->startAnimation(450);

    updateSettingsPageTheme(mode);
}

void MainWindow::applyAcrylic(bool enabled)
{
#ifdef Q_OS_WIN
    setIsCentralStackedWidgetTransparent(enabled);
    const auto targetMode = enabled ? ElaApplicationType::Acrylic : ElaApplicationType::Normal;
    if (eApp->getWindowDisplayMode() == targetMode)
    {
        updateSettingsPageAcrylic(enabled);
        if (m_uiSettings.acrylicEnabled != enabled)
        {
            m_uiSettings.acrylicEnabled = enabled;
            persistUiSettings();
        }
        return;
    }
    eApp->setWindowDisplayMode(targetMode);
#else
    if (enabled && isVisible())
        QMessageBox::information(this, windowTitle(), QStringLiteral(u"当前平台不支持亚克力效果。"));
    setIsCentralStackedWidgetTransparent(false);
    updateSettingsPageAcrylic(false);
    if (m_uiSettings.acrylicEnabled)
    {
        m_uiSettings.acrylicEnabled = false;
        persistUiSettings();
    }
#endif
}
void MainWindow::updateSettingsPageTheme(ElaThemeType::ThemeMode mode)
{
    if (!m_settingsPage)
        return;
    m_settingsPage->setDarkModeChecked(mode == ElaThemeType::Dark);
}

void MainWindow::updateSettingsPageAcrylic(bool enabled)
{
    if (!m_settingsPage)
        return;
    m_settingsPage->setAcrylicChecked(enabled);
}

void MainWindow::updateAccountBanner()
{
    if (!m_accountBanner)
        return;
    m_accountBanner->setText(accountBannerText());
}

QString MainWindow::accountBannerText() const
{
    const QString text = accountBannerTextFor(m_currentUser);
    if (!text.isEmpty())
        return text;
    return QStringLiteral(u"-");
}

void MainWindow::persistUiSettings()
{
    if (!saveUiSettings(m_dataDir, m_uiSettings))
        qWarning() << "Failed to persist settings.json";
}

void MainWindow::loadInitialData()
{
    m_users = m_repository->loadUsers();
    std::sort(m_users.begin(), m_users.end(), userLess);

    auto it = std::find_if(m_users.begin(), m_users.end(), [&](const User &user)
                           { return user.account.compare(m_currentUser.account, Qt::CaseInsensitive) == 0; });
    if (it == m_users.end())
    {
        m_users.push_back(m_currentUser);
        std::sort(m_users.begin(), m_users.end(), userLess);
        it = std::find_if(m_users.begin(), m_users.end(), [&](const User &user)
                          { return user.account.compare(m_currentUser.account, Qt::CaseInsensitive) == 0; });
        m_usersDirty = true;
    }
    if (it != m_users.end())
    {
        m_currentUserIndex = static_cast<int>(std::distance(m_users.begin(), it));
        m_currentUser = *it;
        m_currentBalance = m_currentUser.balance;
    }

    m_sessions = m_repository->loadSessions();
    std::sort(m_sessions.begin(), m_sessions.end(), sessionLess);
    validateSessions();

    m_recharges = m_repository->loadRechargeRecords();
    std::sort(m_recharges.begin(), m_recharges.end(), [](const RechargeRecord &a, const RechargeRecord &b)
              { return a.timestamp > b.timestamp; });

    if (m_billingPage)
        m_billingPage->setOutputDirectory(m_outputDir);

    updateAccountBanner();
}

void MainWindow::validateSessions()
{
    QVector<Session> invalid;
    auto newEnd = std::remove_if(m_sessions.begin(), m_sessions.end(), [&](const Session &session)
                                 {
        const bool broken = !session.begin.isValid() || !session.end.isValid() || session.end <= session.begin;
        if (broken)
            invalid.append(session);
        return broken; });
    if (newEnd == m_sessions.end())
        return;

    m_sessions.erase(newEnd, m_sessions.end());
    m_sessionsDirty = true;

    if (invalid.isEmpty())
        return;

    QDir().mkpath(m_dataDir);
    QFile logFile(m_dataDir + QStringLiteral("/invalid_sessions.log"));
    if (!logFile.open(QIODevice::Append | QIODevice::Text))
        return;
    QTextStream out(&logFile);
    out.setEncoding(QStringConverter::Utf8);
    for (const auto &session : invalid)
    {
        out << session.account << ' '
            << session.begin.toString(Qt::ISODate) << ' '
            << session.end.toString(Qt::ISODate) << '\n';
    }
}

void MainWindow::refreshUsersPage()
{
    if (!m_usersPage)
        return;
    m_usersPage->setUsers(m_users);
}

void MainWindow::refreshSessionsPage()
{
    if (!m_sessionsPage)
        return;

    QHash<QString, QString> names;
    for (const auto &user : m_users)
        names.insert(user.account, user.name);

    m_sessionsPage->setRestrictedAccount(m_isAdmin ? QString() : m_currentUser.account);
    m_sessionsPage->setSessions(m_sessions, names);
}

void MainWindow::refreshBillingSummary()
{
    if (m_isAdmin)
    {
        if (m_billingPage)
        {
            m_billingPage->setBillLines(m_latestBills);
        }
    }
    else if (m_billingPage)
    {
        std::vector<BillLine> mine;
        std::copy_if(m_latestBills.begin(), m_latestBills.end(), std::back_inserter(mine), [&](const BillLine &line)
                     { return line.account.compare(m_currentUser.account, Qt::CaseInsensitive) == 0; });
        m_billingPage->setBillLines(mine);
        if (m_userStatsPage)
            m_userStatsPage->setTrend(m_currentUser.account, collectPersonalTrend(m_currentUser.account));
    }
    else if (m_userStatsPage)
    {
        m_userStatsPage->setTrend(m_currentUser.account, collectPersonalTrend(m_currentUser.account));
    }

    int totalMinutes = 0;
    double totalAmount = 0.0;
    for (const auto &line : m_latestBills)
    {
        totalMinutes += line.minutes;
        totalAmount += line.amount;
    }

    if (m_isAdmin)
    {
        if (m_billingPage)
            m_billingPage->setSummary(totalMinutes, totalAmount, static_cast<int>(m_latestBills.size()));
    }
    else
    {
        int personalMinutes = 0;
        double personalAmount = 0.0;
        for (const auto &line : m_latestBills)
        {
            if (line.account.compare(m_currentUser.account, Qt::CaseInsensitive) == 0)
            {
                personalMinutes += line.minutes;
                personalAmount += line.amount;
            }
        }
        if (m_billingPage)
            m_billingPage->setSummary(personalMinutes, personalAmount, 1);
    }

    int totalSessions = static_cast<int>(m_sessions.size());
    double totalSessionMinutes = 0.0;
    for (const auto &session : m_sessions)
    {
        const qint64 secs = session.begin.secsTo(session.end);
        if (secs > 0)
            totalSessionMinutes += (secs + 59) / 60;
    }

    if (m_dashboardPage)
    {
        m_dashboardPage->updateOverview(m_currentUser,
                                        static_cast<int>(m_users.size()),
                                        totalSessions,
                                        totalSessionMinutes,
                                        totalAmount,
                                        m_currentBalance,
                                        m_lastBillingInfo);
    }

    if (m_reportsPage)
    {
        QVector<int> buckets(4, 0);
        QVector<int> planCounts(5, 0);
        QVector<double> planAmounts(5, 0.0);
        for (const auto &line : m_latestBills)
        {
            if (line.minutes <= 30 * 60)
                ++buckets[0];
            else if (line.minutes <= 60 * 60)
                ++buckets[1];
            else if (line.minutes <= 150 * 60)
                ++buckets[2];
            else
                ++buckets[3];

            const int planIndex = std::clamp(line.plan, 0, 4);
            ++planCounts[planIndex];
            planAmounts[planIndex] += line.amount;
        }
        int year = m_lastBillYear == 0 ? QDate::currentDate().year() : m_lastBillYear;
        int month = m_lastBillMonth == 0 ? QDate::currentDate().month() : m_lastBillMonth;
        const bool hasBillingData = m_hasComputed;
        m_reportsPage->setMonthlySummary(year, month, buckets, totalAmount, hasBillingData);
        m_reportsPage->setPlanDistribution(planCounts, planAmounts);

        double rechargeIncome = 0.0;
        double refundAmount = 0.0;
        for (const auto &record : m_recharges)
        {
            if (record.amount >= 0)
                rechargeIncome += record.amount;
            else
                refundAmount += -record.amount;
        }
        const double netIncome = rechargeIncome - refundAmount;
        m_reportsPage->setFinancialSummary(totalAmount, rechargeIncome, refundAmount, netIncome);
    }
}

QVector<QPair<QString, double>> MainWindow::collectPersonalTrend(const QString &account) const
{
    QVector<QPair<QString, double>> trend;
    if (account.isEmpty())
        return trend;

    constexpr int kTrendWindowMonths = 12;

    if (m_users.empty())
        return trend;

    const auto userIt = std::find_if(m_users.begin(), m_users.end(), [&](const User &user)
                                     { return user.account.compare(account, Qt::CaseInsensitive) == 0; });
    if (userIt == m_users.end())
        return trend;

    QDate earliest;
    for (const auto &session : m_sessions)
    {
        if (session.account.compare(account, Qt::CaseInsensitive) != 0)
            continue;

        const QDate beginDate = session.begin.date();
        const QDate endDate = session.end.date();

        const QDate beginFirst(beginDate.year(), beginDate.month(), 1);
        if (!earliest.isValid() || beginFirst < earliest)
            earliest = beginFirst;

        const QDate endFirst(endDate.year(), endDate.month(), 1);
        if (!earliest.isValid() || endFirst < earliest)
            earliest = endFirst;
    }

    if (!earliest.isValid())
        return trend;

    QDate anchor;
    if (m_lastBillYear > 0 && m_lastBillMonth > 0)
        anchor = QDate(m_lastBillYear, m_lastBillMonth, 1);
    else
        anchor = QDate::currentDate();
    anchor = QDate(anchor.year(), anchor.month(), 1);

    QDate start = anchor.addMonths(-(kTrendWindowMonths - 1));
    if (earliest.isValid() && earliest > start)
        start = earliest;

    for (QDate month = start; month <= anchor; month = month.addMonths(1))
    {
        const auto bills = BillingEngine::computeMonthly(month.year(), month.month(), m_users, m_sessions);
        const auto it = std::find_if(bills.begin(), bills.end(), [&](const BillLine &line)
                                     { return line.account.compare(account, Qt::CaseInsensitive) == 0; });
        const double amount = (it != bills.end()) ? it->amount : 0.0;
        trend.append({month.toString(QStringLiteral("yyyy-MM")), amount});
    }

    while (!trend.isEmpty() && qFuzzyIsNull(trend.constFirst().second))
        trend.remove(0);

    if (trend.isEmpty())
        return trend;

    if (trend.size() > kTrendWindowMonths)
        trend = trend.mid(trend.size() - kTrendWindowMonths);

    return trend;
}

void MainWindow::refreshRechargePage()
{
    if (!m_rechargePage)
        return;

    m_rechargePage->setAdminMode(m_isAdmin);
    m_rechargePage->setUsers(m_users);
    m_rechargePage->setCurrentAccount(m_currentUser.account);
    m_rechargePage->setCurrentBalance(m_currentBalance);

    if (m_isAdmin)
    {
        m_rechargePage->setRechargeRecords(m_recharges);
    }
    else
    {
        std::vector<RechargeRecord> mine;
        std::copy_if(m_recharges.begin(), m_recharges.end(), std::back_inserter(mine), [&](const RechargeRecord &record)
                     { return record.account.compare(m_currentUser.account, Qt::CaseInsensitive) == 0; });
        m_rechargePage->setRechargeRecords(mine);
    }
}

void MainWindow::resetComputedBills()
{
    m_latestBills.clear();
    m_hasComputed = false;
    m_lastBillYear = 0;
    m_lastBillMonth = 0;
    m_lastBillingInfo.clear();
}

void MainWindow::handleCreateUser()
{
    if (!m_isAdmin || !m_usersPage)
        return;

    UserEditorDialog dialog(UserEditorDialog::Mode::Create, this);
    QStringList existingAccounts;
    existingAccounts.reserve(static_cast<int>(m_users.size()));
    for (const User &user : std::as_const(m_users))
        existingAccounts.append(user.account);
    dialog.setExistingAccounts(existingAccounts);
    if (dialog.exec() != QDialog::Accepted)
        return;

    User user = dialog.user();
    if (user.account.trimmed().isEmpty())
        return;

    const bool exists = std::any_of(m_users.begin(), m_users.end(), [&](const User &u)
                                    { return u.account.compare(user.account, Qt::CaseInsensitive) == 0; });
    if (exists)
    {
        QMessageBox::warning(this, windowTitle(), QStringLiteral(u"账号已存在，请使用唯一账号。"));
        return;
    }

    m_users.push_back(user);
    std::sort(m_users.begin(), m_users.end(), userLess);
    m_usersDirty = true;
    refreshUsersPage();
}

void MainWindow::handleEditUser(const QString &account)
{
    if (!m_isAdmin || !m_usersPage)
        return;

    auto it = std::find_if(m_users.begin(), m_users.end(), [&](const User &u)
                           { return u.account.compare(account, Qt::CaseInsensitive) == 0; });
    if (it == m_users.end())
    {
        QMessageBox::warning(this, windowTitle(), QStringLiteral(u"未找到选中的账号。"));
        return;
    }

    UserEditorDialog dialog(UserEditorDialog::Mode::Edit, this);
    QStringList existingAccounts;
    existingAccounts.reserve(static_cast<int>(m_users.size()));
    for (const User &candidate : std::as_const(m_users))
    {
        if (candidate.account.compare(account, Qt::CaseInsensitive) == 0)
            continue;
        existingAccounts.append(candidate.account);
    }
    dialog.setExistingAccounts(existingAccounts);
    dialog.setUser(*it);
    if (dialog.exec() != QDialog::Accepted)
        return;

    *it = dialog.user();
    if (m_currentUser.account.compare(it->account, Qt::CaseInsensitive) == 0)
    {
        m_currentUser = *it;
        m_currentBalance = it->balance;
        updateAccountBanner();
    }
    std::sort(m_users.begin(), m_users.end(), userLess);
    m_usersDirty = true;
    refreshUsersPage();
    refreshBillingSummary();
    refreshRechargePage();
}

void MainWindow::handleDeleteUsers(const QStringList &accounts)
{
    if (!m_isAdmin || !m_usersPage || accounts.isEmpty())
        return;

    for (const QString &account : accounts)
    {
        if (account.compare(m_currentUser.account, Qt::CaseInsensitive) == 0)
        {
            QMessageBox::warning(this, windowTitle(), QStringLiteral(u"不能删除当前登录账号。"));
            return;
        }
    }

    if (QMessageBox::question(this, windowTitle(),
                              QStringLiteral(u"确认要删除选中的 %1 位用户吗？\n关联的上网记录也会一并移除。")
                                  .arg(accounts.size())) != QMessageBox::Yes)
        return;

    QSet<QString> targets;
    for (const QString &account : accounts)
        targets.insert(account.toLower());

    m_users.erase(std::remove_if(m_users.begin(), m_users.end(), [&](const User &user)
                                 { return targets.contains(user.account.toLower()); }),
                  m_users.end());

    const auto sessionsSizeBefore = m_sessions.size();
    m_sessions.erase(std::remove_if(m_sessions.begin(), m_sessions.end(), [&](const Session &session)
                                    { return targets.contains(session.account.toLower()); }),
                     m_sessions.end());
    if (m_sessions.size() != sessionsSizeBefore)
        m_sessionsDirty = true;

    m_usersDirty = true;
    refreshUsersPage();
    refreshSessionsPage();

    auto currentIt = std::find_if(m_users.begin(), m_users.end(), [&](const User &user)
                                  { return user.account.compare(m_currentUser.account, Qt::CaseInsensitive) == 0; });
    if (currentIt != m_users.end())
    {
        m_currentUserIndex = static_cast<int>(std::distance(m_users.begin(), currentIt));
        m_currentUser = *currentIt;
        m_currentBalance = currentIt->balance;
        updateAccountBanner();
    }
}

void MainWindow::handleReloadUsers()
{
    if (!m_isAdmin || !m_usersPage)
        return;

    if (m_usersDirty)
    {
        if (QMessageBox::question(this, windowTitle(), QStringLiteral(u"有未保存的用户信息，确认放弃修改并重新加载吗？")) != QMessageBox::Yes)
            return;
    }

    loadInitialData();
    refreshUsersPage();
    refreshSessionsPage();
    refreshBillingSummary();
    refreshRechargePage();
    m_usersDirty = false;
}

void MainWindow::handleSaveUsers()
{
    if (!m_repository || !m_usersPage)
        return;
    if (!persistUsers())
    {
        QMessageBox::warning(this, windowTitle(), QStringLiteral(u"保存用户信息失败。"));
        return;
    }
    QMessageBox::information(this, windowTitle(), QStringLiteral(u"用户信息已保存。"));
}

void MainWindow::handleCreateSession()
{
    if (!m_isAdmin || !m_sessionsPage)
        return;

    QHash<QString, QString> names;
    for (const auto &user : m_users)
        names.insert(user.account, user.name);

    SessionEditorDialog dialog(names, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    Session session = dialog.session();
    if (session.account.isEmpty())
        return;

    m_sessions.push_back(session);
    std::sort(m_sessions.begin(), m_sessions.end(), sessionLess);
    m_sessionsDirty = true;
    refreshSessionsPage();
    resetComputedBills();
}

void MainWindow::handleEditSession(const Session &session)
{
    if (!m_isAdmin || !m_sessionsPage)
        return;

    auto it = std::find_if(m_sessions.begin(), m_sessions.end(), [&](const Session &s)
                           { return sessionsEqual(s, session); });
    if (it == m_sessions.end())
    {
        QMessageBox::warning(this, windowTitle(), QStringLiteral(u"未找到选中的上网记录。"));
        return;
    }

    QHash<QString, QString> names;
    for (const auto &user : m_users)
        names.insert(user.account, user.name);

    SessionEditorDialog dialog(names, this);
    dialog.setSession(*it);
    if (dialog.exec() != QDialog::Accepted)
        return;

    *it = dialog.session();
    std::sort(m_sessions.begin(), m_sessions.end(), sessionLess);
    m_sessionsDirty = true;
    refreshSessionsPage();
    resetComputedBills();
}

void MainWindow::handleDeleteSessions(const QList<Session> &sessions)
{
    if (!m_isAdmin || !m_sessionsPage || sessions.isEmpty())
        return;

    if (QMessageBox::question(this, windowTitle(),
                              QStringLiteral(u"确认要删除选中的 %1 条上网记录吗？").arg(sessions.size())) != QMessageBox::Yes)
        return;

    for (const auto &session : sessions)
    {
        m_sessions.erase(std::remove_if(m_sessions.begin(),
                                        m_sessions.end(),
                                        [&](const Session &s)
                                        { return sessionsEqual(s, session); }),
                         m_sessions.end());
    }
    m_sessionsDirty = true;
    refreshSessionsPage();
    resetComputedBills();
}

void MainWindow::handleReloadSessions()
{
    if (!m_sessionsPage)
        return;

    if (m_sessionsDirty)
    {
        if (QMessageBox::question(this, windowTitle(), QStringLiteral(u"有未保存的上网记录，确认要放弃修改并重新加载吗？")) != QMessageBox::Yes)
            return;
    }

    m_sessions = m_repository->loadSessions();
    std::sort(m_sessions.begin(), m_sessions.end(), sessionLess);
    m_sessionsDirty = false;
    refreshSessionsPage();
    resetComputedBills();
}

void MainWindow::handleSaveSessions()
{
    if (!persistSessions())
    {
        QMessageBox::warning(this, windowTitle(), QStringLiteral(u"保存上网记录失败。"));
        return;
    }
    QMessageBox::information(this, windowTitle(), QStringLiteral(u"上网记录已保存。"));
}

void MainWindow::handleGenerateRandomSessions()
{
    if (!m_isAdmin || !m_sessionsPage)
        return;

    if (m_users.empty())
    {
        QMessageBox::warning(this, windowTitle(), QStringLiteral(u"请先创建至少一位用户。"));
        return;
    }

    auto *rng = QRandomGenerator::global();
    const QDate today = QDate::currentDate();
    const QDate rangeStart(today.year() - 1, 1, 1);
    const QDate rangeEnd = today.addMonths(2);
    const int daySpan = rangeStart.daysTo(rangeEnd);

    auto appendSession = [&](const QString &account, const QDateTime &begin, int minutes)
    {
        const QDateTime end = begin.addSecs(minutes * 60);
        if (end <= begin)
            return;
        m_sessions.push_back(Session{account, begin, end});
    };

    for (const auto &user : m_users)
    {
        const int count = rng->bounded(5, 15);
        for (int i = 0; i < count; ++i)
        {
            const QDate day = rangeStart.addDays(rng->bounded(daySpan));
            const int startMinutes = rng->bounded(0, 24 * 60 - 30);
            const int duration = rng->bounded(15, 240);

            const QTime beginTime = QTime::fromMSecsSinceStartOfDay(startMinutes * 60 * 1000);
            const QDateTime begin(day, beginTime);
            appendSession(user.account, begin, duration);
        }

        // inject a cross-month session (e.g. end of month spilling into next month)
        const QDate crossMonthBase = QDate(today.year(), today.month(), 1).addMonths(-rng->bounded(1, 4));
        const QDate crossMonthDay = crossMonthBase.addMonths(1).addDays(-1);
        const QTime crossMonthStart(23, rng->bounded(0, 45));
        const int crossMonthDuration = rng->bounded(90, 240);
        appendSession(user.account, QDateTime(crossMonthDay, crossMonthStart), crossMonthDuration);

        // inject a cross-year session (Dec 31 spilling into Jan 1 of next year)
        const QDate crossYearDate(today.year() - 1, 12, 31);
        if (crossYearDate.isValid())
        {
            const QTime crossYearStart(22, rng->bounded(0, 60));
            const int crossYearDuration = rng->bounded(120, 360);
            appendSession(user.account, QDateTime(crossYearDate, crossYearStart), crossYearDuration);
        }
    }

    std::sort(m_sessions.begin(), m_sessions.end(), sessionLess);
    m_sessionsDirty = true;
    refreshSessionsPage();
    resetComputedBills();
    QMessageBox::information(this, windowTitle(), QStringLiteral(u"已追加随机生成的上网记录，请检查并保存。"));
}

void MainWindow::handleChangePasswordRequest()
{
    ChangePasswordDialog dialog(this);
    dialog.setAccount(m_currentUser.account);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const QString account = dialog.account();
    if (account.isEmpty())
    {
        QMessageBox::warning(this, windowTitle(), QStringLiteral(u"账号不能为空。"));
        return;
    }

    if (!m_isAdmin && account.compare(m_currentUser.account, Qt::CaseInsensitive) != 0)
    {
        QMessageBox::warning(this, windowTitle(), QStringLiteral(u"仅能修改当前登录账号的密码。"));
        return;
    }

    auto it = std::find_if(m_users.begin(), m_users.end(), [&](const User &user)
                           { return user.account.compare(account, Qt::CaseInsensitive) == 0; });
    if (it == m_users.end())
    {
        QMessageBox::warning(this, windowTitle(), QStringLiteral(u"账号不存在。"));
        return;
    }

    if (!Security::verifyPassword(dialog.oldPassword(), it->passwordHash))
    {
        QMessageBox::warning(this, windowTitle(), QStringLiteral(u"当前密码不正确。"));
        return;
    }

    const QString oldHash = it->passwordHash;
    it->passwordHash = Security::hashPassword(dialog.newPassword());

    // Update in-memory current user first so persistUsers writes the correct hash
    if (m_currentUser.account.compare(account, Qt::CaseInsensitive) == 0)
    {
        m_currentUser.passwordHash = it->passwordHash;
    }

    if (!persistUsers())
    {
        it->passwordHash = oldHash;
        // restore current user hash as well
        if (m_currentUser.account.compare(account, Qt::CaseInsensitive) == 0)
        {
            m_currentUser.passwordHash = oldHash;
        }
        QMessageBox::warning(this, windowTitle(), QStringLiteral(u"保存密码失败，请稍后重试。"));
        return;
    }

    refreshUsersPage();
    QMessageBox::information(this, windowTitle(), QStringLiteral(u"密码已更新。"));
}

void MainWindow::handleSwitchAccountRequested()
{
    const auto decision = QMessageBox::question(
        this,
        windowTitle(),
        QStringLiteral(u"确定要切换账号吗？"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (decision != QMessageBox::Yes)
        return;

    hide();

    LoginDialog loginDialog(m_dataDir, m_outputDir);
    const int result = loginDialog.exec();

    if (result == QDialog::Accepted && loginDialog.isAuthenticated())
    {
        auto *nextWindow = new MainWindow(loginDialog.authenticatedUser(), loginDialog.dataDir(), loginDialog.outputDir());
        nextWindow->moveToCenter();
        nextWindow->show();
        close();
        return;
    }

    show();
    raise();
    activateWindow();
}

void MainWindow::handleBackupRequested()
{
    if (!m_isAdmin || !m_repository)
        return;

    if (m_usersDirty && !persistUsers())
    {
        QMessageBox::warning(this, windowTitle(), QStringLiteral(u"保存用户数据失败，已取消备份。"));
        return;
    }

    if (m_sessionsDirty && !persistSessions())
    {
        QMessageBox::warning(this, windowTitle(), QStringLiteral(u"保存上网记录失败，已取消备份。"));
        return;
    }

    if (!m_repository->saveRechargeRecords(m_recharges))
    {
        QMessageBox::warning(this, windowTitle(), QStringLiteral(u"写入充值流水失败，已取消备份。"));
        return;
    }

    const QString defaultName = QStringLiteral("NetBilling-%1.nbbak").arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss")));
    QString target = QFileDialog::getSaveFileName(this,
                                                  QStringLiteral(u"导出数据备份"),
                                                  defaultName,
                                                  QStringLiteral(u"NetBilling 备份 (*.nbbak);;所有文件 (*.*)"));
    if (target.isEmpty())
        return;

    if (QFileInfo(target).suffix().isEmpty())
        target.append(QStringLiteral(".nbbak"));

    QString error;
    if (!m_repository->exportBackup(target, &error))
    {
        if (error.isEmpty())
            error = QStringLiteral(u"导出备份失败。");
        QMessageBox::warning(this, windowTitle(), error);
        return;
    }

    QMessageBox::information(this, windowTitle(),
                             QStringLiteral(u"数据备份已导出至：\n%1").arg(QDir::toNativeSeparators(target)));
}

void MainWindow::handleRestoreRequested()
{
    if (!m_isAdmin || !m_repository)
        return;

    const bool hasUnsaved = m_usersDirty || m_sessionsDirty;
    const QString prompt = hasUnsaved
                               ? QStringLiteral(u"检测到存在尚未保存的修改，恢复备份将覆盖当前数据。\n确定继续吗？")
                               : QStringLiteral(u"恢复备份将覆盖当前数据，确定继续吗？");

    if (QMessageBox::question(this, windowTitle(), prompt, QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;

    const QString source = QFileDialog::getOpenFileName(this,
                                                        QStringLiteral(u"导入数据备份"),
                                                        QString(),
                                                        QStringLiteral(u"NetBilling 备份 (*.nbbak *.json);;所有文件 (*.*)"));
    if (source.isEmpty())
        return;

    QString error;
    if (!m_repository->importBackup(source, &error))
    {
        if (error.isEmpty())
            error = QStringLiteral(u"导入备份失败。");
        QMessageBox::warning(this, windowTitle(), error);
        return;
    }

    m_usersDirty = false;
    m_sessionsDirty = false;

    loadInitialData();
    refreshUsersPage();
    refreshSessionsPage();
    resetComputedBills();
    refreshBillingSummary();
    refreshRechargePage();

    QMessageBox::information(this, windowTitle(), QStringLiteral(u"数据已从备份中恢复。"));
}

void MainWindow::handleStackIndexChanged()
{
    if (!m_isAdmin || !m_billingPage)
        return;

    const QString currentKey = getCurrentNavigationPageKey();
    const bool isBillingPage = (!m_billingPageKey.isEmpty() && currentKey == m_billingPageKey) ||
                               (m_billingPageKey.isEmpty() && m_billingPage->isVisible());
    if (!isBillingPage || m_hasComputed || !m_repository)
        return;

    QTimer::singleShot(0, this, [this]()
                       {
                           if (!m_hasComputed)
                               handleComputeBilling(); });
}

void MainWindow::handleComputeBilling()
{
    if (!m_billingPage)
        return;

    const int year = m_billingPage->selectedYear();
    const int month = m_billingPage->selectedMonth();

    if (!m_isAdmin)
    {
        const auto allBills = BillingEngine::computeMonthly(year, month, m_users, m_sessions);
        std::vector<BillLine> mine;
        std::copy_if(allBills.begin(), allBills.end(), std::back_inserter(mine), [&](const BillLine &line)
                     { return line.account.compare(m_currentUser.account, Qt::CaseInsensitive) == 0; });

        m_latestBills = mine;
        m_hasComputed = true;
        m_lastBillYear = year;
        m_lastBillMonth = month;

        const QLocale locale(QLocale::Chinese, QLocale::China);
        double totalAmount = 0.0;
        for (const auto &line : mine)
            totalAmount += line.amount;

        if (!mine.empty())
        {
            m_lastBillingInfo = QStringLiteral(u"最新账单：%1 年 %2 月合计 %3 元")
                                    .arg(year)
                                    .arg(month, 2, 10, QLatin1Char('0'))
                                    .arg(locale.toString(totalAmount, 'f', 2));
        }
        else
        {
            m_lastBillingInfo = QStringLiteral(u"最新账单：%1 年 %2 月暂无数据。")
                                    .arg(year)
                                    .arg(month, 2, 10, QLatin1Char('0'));
        }

        refreshBillingSummary();
        return;
    }

    if (!m_repository)
        return;

    const QString dirFromUi = m_billingPage->outputDirectory();
    if (!dirFromUi.isEmpty() && dirFromUi != m_outputDir)
    {
        m_outputDir = dirFromUi;
        ensureOutputDir();
        m_repository = std::make_unique<Repository>(m_dataDir, m_outputDir);
        if (m_billingPage)
            m_billingPage->setOutputDirectory(m_outputDir);
    }

    m_latestBills = BillingEngine::computeMonthly(year, month, m_users, m_sessions);
    m_hasComputed = true;
    m_lastBillYear = year;
    m_lastBillMonth = month;

    QVector<QString> negativeAccounts;
    const QDateTime timestamp = QDateTime::currentDateTime();
    double totalAmount = 0.0;

    for (auto &line : m_latestBills)
    {
        totalAmount += line.amount;

        auto it = std::find_if(m_users.begin(), m_users.end(), [&](const User &user)
                               { return user.account.compare(line.account, Qt::CaseInsensitive) == 0; });
        if (it == m_users.end())
            continue;

        it->balance -= line.amount;
        if (it->balance < 0)
            negativeAccounts.append(it->account);

        RechargeRecord deduction{line.account, timestamp, -line.amount, m_currentUser.account, QStringLiteral(u"月度扣费"), it->balance};
        m_recharges.insert(m_recharges.begin(), deduction);
    }

    auto refreshCurrent = [&]()
    {
        auto currentIt = std::find_if(m_users.begin(), m_users.end(), [&](const User &user)
                                      { return user.account.compare(m_currentUser.account, Qt::CaseInsensitive) == 0; });
        if (currentIt != m_users.end())
        {
            m_currentUserIndex = static_cast<int>(std::distance(m_users.begin(), currentIt));
            m_currentUser = *currentIt;
            m_currentBalance = currentIt->balance;
            updateAccountBanner();
        }
    };
    refreshCurrent();

    m_lastBillingInfo = QStringLiteral(u"最新账单：%1 年 %2 月合计 %3 元")
                            .arg(year)
                            .arg(month, 2, 10, QLatin1Char('0'))
                            .arg(QString::number(totalAmount, 'f', 2));

    if (!persistUsers())
        QMessageBox::warning(this, windowTitle(), QStringLiteral(u"保存用户余额失败，请稍后重试。"));

    if (m_repository && !m_repository->saveRechargeRecords(m_recharges))
        QMessageBox::warning(this, windowTitle(), QStringLiteral(u"写入充值流水失败，请检查数据目录权限。"));

    if (m_repository && !m_repository->writeMonthlyBill(year, month, m_latestBills))
    {
        QMessageBox::warning(this, windowTitle(), QStringLiteral(u"账单写入失败，请检查输出目录。"));
    }

    if (!negativeAccounts.isEmpty())
    {
        QMessageBox::warning(this, windowTitle(),
                             QStringLiteral(u"以下账号余额已为负数，请及时充值：\n%1")
                                 .arg(negativeAccounts.join(QStringLiteral(", "))));
    }

    refreshCurrent();
    refreshBillingSummary();
    refreshRechargePage();
    refreshUsersPage();
}

void MainWindow::handleExportBilling()
{
    if (!m_repository || !m_hasComputed || m_latestBills.empty())
    {
        QMessageBox::information(this, windowTitle(), QStringLiteral(u"请先生成账单再导出。"));
        return;
    }

    const QString dirFromUi = m_billingPage ? m_billingPage->outputDirectory() : m_outputDir;
    if (!dirFromUi.isEmpty() && dirFromUi != m_outputDir)
    {
        m_outputDir = dirFromUi;
        ensureOutputDir();
        m_repository = std::make_unique<Repository>(m_dataDir, m_outputDir);
        if (m_billingPage)
            m_billingPage->setOutputDirectory(m_outputDir);
    }

    ensureOutputDir();
    if (m_repository->writeMonthlyBill(m_lastBillYear, m_lastBillMonth, m_latestBills))
    {
        const QString fileName = QStringLiteral("%1/bill_%2_%3.csv")
                                     .arg(m_outputDir)
                                     .arg(m_lastBillYear)
                                     .arg(m_lastBillMonth, 2, 10, QLatin1Char('0'));
        QMessageBox::information(this, windowTitle(),
                                 QStringLiteral(u"账单已导出至：\n%1")
                                     .arg(QDir::toNativeSeparators(fileName)));
    }
    else
    {
        QMessageBox::warning(this, windowTitle(), QStringLiteral(u"写入账单文件失败，请检查目录权限。"));
    }
}

void MainWindow::handleRecharge(const QString &account, double amount, const QString &note, bool selfService)
{
    if (account.isEmpty())
        return;

    if (selfService && amount <= 0)
    {
        QMessageBox::warning(this, windowTitle(), QStringLiteral(u"充值金额必须大于 0。"));
        return;
    }

    auto it = std::find_if(m_users.begin(), m_users.end(), [&](const User &user)
                           { return user.account.compare(account, Qt::CaseInsensitive) == 0; });
    if (it == m_users.end())
    {
        QMessageBox::warning(this, windowTitle(), QStringLiteral(u"账号不存在。"));
        return;
    }

    it->balance += amount;
    if (m_currentUser.account.compare(account, Qt::CaseInsensitive) == 0)
    {
        m_currentUser = *it;
        m_currentBalance = it->balance;
        updateAccountBanner();
    }

    RechargeRecord record{account,
                          QDateTime::currentDateTime(),
                          amount,
                          m_currentUser.account,
                          note,
                          it->balance};
    m_recharges.insert(m_recharges.begin(), record);

    if (!persistUsers())
    {
        QMessageBox::warning(this, windowTitle(), QStringLiteral(u"保存用户余额失败，请稍后重试。"));
        return;
    }

    if (!m_repository->appendRechargeRecord(record))
    {
        QMessageBox::warning(this, windowTitle(), QStringLiteral(u"记录充值流水失败，请检查数据目录权限。"));
    }

    refreshRechargePage();
    refreshUsersPage();
    refreshBillingSummary();
    QMessageBox::information(this, windowTitle(), QStringLiteral(u"余额已更新。"));
}

QString MainWindow::defaultOutputDir() const
{
    return QDir::current().filePath(QStringLiteral("out"));
}

void MainWindow::ensureOutputDir()
{
    if (m_outputDir.isEmpty())
        m_outputDir = defaultOutputDir();
    QDir dir(m_outputDir);
    if (!dir.exists())
        QDir().mkpath(m_outputDir);
}

bool MainWindow::persistUsers()
{
    if (!m_repository)
        return false;
    if (m_currentUserIndex >= 0 && m_currentUserIndex < static_cast<int>(m_users.size()))
        m_users[static_cast<std::size_t>(m_currentUserIndex)] = m_currentUser;
    const bool ok = m_repository->saveUsers(m_users);
    if (ok)
        m_usersDirty = false;
    return ok;
}

bool MainWindow::persistSessions()
{
    if (!m_repository)
        return false;
    const bool ok = m_repository->saveSessions(m_sessions);
    if (ok)
        m_sessionsDirty = false;
    return ok;
}
