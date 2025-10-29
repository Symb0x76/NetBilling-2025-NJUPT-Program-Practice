#include "ui/mainwindow.h"

#include "backend/billing.h"
#include "backend/repository.h"
#include "backend/security.h"
#include "ui/dialogs/session_editor_dialog.h"
#include "ui/dialogs/user_editor_dialog.h"
#include "ui/pages/billing_page.h"
#include "ui/pages/dashboard_page.h"
#include "ui/pages/recharge_page.h"
#include "ui/pages/reports_page.h"
#include "ui/pages/sessions_page.h"
#include "ui/pages/users_page.h"

#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QHash>
#include <QIcon>
#include <QMessageBox>
#include <QMetaType>
#include <QRandomGenerator>
#include <QSet>
#include <QTextStream>
#include <QStringConverter>
#include <QTime>
#include <QVector>

#include <algorithm>
#include <iterator>
#include <numeric>

namespace
{
    QString accountDisplayName(const User &user)
    {
        return user.name.isEmpty() ? user.account : QStringLiteral("%1 (%2)").arg(user.name, user.account);
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

    setupUi();
    setupNavigation();
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
    setWindowIcon(QIcon(QStringLiteral(":/icons/app.svg")));

    setNavigationBarWidth(260);
    setNavigationBarDisplayMode(ElaNavigationType::Compact);
    setUserInfoCardVisible(true);
    setUserInfoCardTitle(accountDisplayName(m_currentUser));
    setUserInfoCardSubTitle(m_isAdmin ? QStringLiteral(u"管理员") : QStringLiteral(u"普通用户"));
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

        addExpanderNode(QStringLiteral(u"数据维护"), m_dataGroupKey, ElaIconType::Database);
        addPageNode(QStringLiteral(u"用户管理"), m_usersPage.get(), m_dataGroupKey, ElaIconType::PeopleGroup);
        addPageNode(QStringLiteral(u"上网记录"), m_sessionsPage.get(), m_dataGroupKey, ElaIconType::Timeline);
        expandNavigationNode(m_dataGroupKey);

        addPageNode(QStringLiteral(u"账单结算"), m_billingPage.get(), ElaIconType::Calculator);
        addPageNode(QStringLiteral(u"余额充值"), m_rechargePage.get(), ElaIconType::Wallet);

        addExpanderNode(QStringLiteral(u"分析报表"), m_reportsGroupKey, ElaIconType::ChartLine);
        addPageNode(QStringLiteral(u"统计分析"), m_reportsPage.get(), m_reportsGroupKey, ElaIconType::ChartColumn);
    }
    else
    {
        m_sessionsPage = std::make_unique<SessionsPage>(this);
        m_billingPage = std::make_unique<BillingPage>(this);
        m_rechargePage = std::make_unique<RechargePage>(this);

        addPageNode(QStringLiteral(u"上网记录"), m_sessionsPage.get(), ElaIconType::Timeline);
        addPageNode(QStringLiteral(u"账单概览"), m_billingPage.get(), ElaIconType::Calculator);
        addPageNode(QStringLiteral(u"余额充值"), m_rechargePage.get(), ElaIconType::Wallet);
    }

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

    if (m_rechargePage)
    {
        connect(m_rechargePage.get(), &RechargePage::rechargeRequested, this, &MainWindow::handleRecharge);
    }
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
            m_billingPage->setBillLines(m_latestBills);
    }
    else if (m_billingPage)
    {
        std::vector<BillLine> mine;
        std::copy_if(m_latestBills.begin(), m_latestBills.end(), std::back_inserter(mine), [&](const BillLine &line)
                     { return line.account.compare(m_currentUser.account, Qt::CaseInsensitive) == 0; });
        m_billingPage->setBillLines(mine);
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
        }
        int year = m_lastBillYear == 0 ? QDate::currentDate().year() : m_lastBillYear;
        int month = m_lastBillMonth == 0 ? QDate::currentDate().month() : m_lastBillMonth;
        m_reportsPage->setMonthlySummary(year, month, buckets, totalAmount);
    }
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
    dialog.setUser(*it);
    if (dialog.exec() != QDialog::Accepted)
        return;

    *it = dialog.user();
    if (m_currentUser.account.compare(it->account, Qt::CaseInsensitive) == 0)
    {
        m_currentUser = *it;
        m_currentBalance = it->balance;
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
    const QDate baseDate = QDate::currentDate().addMonths(-2).addDays(1 - QDate::currentDate().day());
    const int daySpan = 120;

    for (const auto &user : m_users)
    {
        const int count = rng->bounded(5, 15);
        for (int i = 0; i < count; ++i)
        {
            const QDate day = baseDate.addDays(rng->bounded(daySpan));
            const int startMinutes = rng->bounded(0, 24 * 60 - 30);
            const int duration = rng->bounded(15, 240);

            const QTime beginTime = QTime::fromMSecsSinceStartOfDay(startMinutes * 60 * 1000);
            const QDateTime begin(day, beginTime);
            const QDateTime end = begin.addSecs(duration * 60);

            m_sessions.push_back(Session{user.account, begin, end});
        }
    }

    std::sort(m_sessions.begin(), m_sessions.end(), sessionLess);
    m_sessionsDirty = true;
    refreshSessionsPage();
    resetComputedBills();
    QMessageBox::information(this, windowTitle(), QStringLiteral(u"已追加随机生成的上网记录，请检查并保存。"));
}

void MainWindow::handleComputeBilling()
{
    if (!m_billingPage || !m_repository)
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

    const int year = m_billingPage->selectedYear();
    const int month = m_billingPage->selectedMonth();

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
        const QString fileName = QStringLiteral("%1/bill_%2_%3.txt")
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
