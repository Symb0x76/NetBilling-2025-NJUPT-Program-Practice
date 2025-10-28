#include "ui/mainwindow.h"

#include "backend/billing.h"
#include "backend/repository.h"
#include "ui/dialogs/session_editor_dialog.h"
#include "ui/dialogs/user_editor_dialog.h"
#include "ui/pages/billing_page.h"
#include "ui/pages/dashboard_page.h"
#include "ui/pages/reports_page.h"
#include "ui/pages/sessions_page.h"
#include "ui/pages/users_page.h"

#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QTime>
#include <QHash>
#include <QIcon>
#include <QMessageBox>
#include <QMetaType>
#include <QRandomGenerator>
#include <QSet>
#include <QVector>
#include <algorithm>
#include <numeric>

namespace
{
bool userLess(const User& a, const User& b)
{
    return a.account < b.account;
}

bool sessionLess(const Session& a, const Session& b)
{
    if (a.account != b.account)
        return a.account < b.account;
    if (a.begin != b.begin)
        return a.begin < b.begin;
    return a.end < b.end;
}

bool sessionsEqual(const Session& lhs, const Session& rhs)
{
    return lhs.account == rhs.account && lhs.begin == rhs.begin && lhs.end == rhs.end;
}
} // namespace

MainWindow::MainWindow(QWidget* parent)
    : ElaWindow(parent)
{
    qRegisterMetaType<Session>("Session");
    qRegisterMetaType<QList<Session>>("QList<Session>");

    setupUi();
    setupNavigation();
    connectSignals();

    m_outputDir = defaultOutputDir();
    ensureOutputDir();
    if (m_billingPage)
        m_billingPage->setOutputDirectory(m_outputDir);

    m_dataDir = QDir::current().filePath(QStringLiteral("data"));
    m_repository = std::make_unique<Repository>(m_dataDir, m_outputDir);

    loadInitialData();
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUi()
{
    setWindowTitle(QStringLiteral(u8"上网计费系统"));
    setMinimumSize({1100, 720});
    resize(1280, 800);
    setWindowIcon(QIcon(QStringLiteral(":/icons/app.svg")));

    setNavigationBarWidth(260);
    setNavigationBarDisplayMode(ElaNavigationType::Inline);
    setUserInfoCardVisible(true);
    setUserInfoCardTitle(QStringLiteral(u8"NetBilling"));
    setUserInfoCardSubTitle(QStringLiteral(u8"校园/企业上网计费助手"));
}

void MainWindow::setupNavigation()
{
    m_dashboardPage = std::make_unique<DashboardPage>(this);
    m_usersPage = std::make_unique<UsersPage>(this);
    m_sessionsPage = std::make_unique<SessionsPage>(this);
    m_billingPage = std::make_unique<BillingPage>(this);
    m_reportsPage = std::make_unique<ReportsPage>(this);

    addPageNode(QStringLiteral(u8"仪表盘"), m_dashboardPage.get(), ElaIconType::GaugeHigh);

    addExpanderNode(QStringLiteral(u8"数据维护"), m_dataGroupKey, ElaIconType::Database);
    addPageNode(QStringLiteral(u8"用户管理"), m_usersPage.get(), m_dataGroupKey, ElaIconType::PeopleGroup);
    addPageNode(QStringLiteral(u8"上网记录"), m_sessionsPage.get(), m_dataGroupKey, ElaIconType::Timeline);
    expandNavigationNode(m_dataGroupKey);

    addPageNode(QStringLiteral(u8"账单结算"), m_billingPage.get(), ElaIconType::Calculator);

    addExpanderNode(QStringLiteral(u8"分析报表"), m_reportsGroupKey, ElaIconType::ChartLine);
    addPageNode(QStringLiteral(u8"统计分析"), m_reportsPage.get(), m_reportsGroupKey, ElaIconType::ChartColumn);

    setCurrentStackIndex(0);
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
        connect(m_billingPage.get(), &BillingPage::requestBrowseOutputDir, this, [this]() {
            const QString selected = QFileDialog::getExistingDirectory(this, QStringLiteral(u8"选择导出目录"), m_outputDir);
            if (selected.isEmpty())
                return;
            m_outputDir = selected;
            ensureOutputDir();
            if (m_billingPage)
                m_billingPage->setOutputDirectory(m_outputDir);
            m_repository = std::make_unique<Repository>(m_dataDir, m_outputDir);
            resetComputedBills();
        });
    }
}

void MainWindow::loadInitialData()
{
    if (!m_repository)
        return;

    m_users = m_repository->loadUsers();
    std::sort(m_users.begin(), m_users.end(), userLess);

    m_sessions = m_repository->loadSessions();
    std::sort(m_sessions.begin(), m_sessions.end(), sessionLess);
    m_sessions.erase(std::unique(m_sessions.begin(), m_sessions.end(), sessionsEqual), m_sessions.end());

    m_usersDirty = false;
    m_sessionsDirty = false;

    refreshUsersPage();
    refreshSessionsPage();
    resetComputedBills();
}

void MainWindow::refreshUsersPage()
{
    if (!m_usersPage)
        return;
    std::sort(m_users.begin(), m_users.end(), userLess);
    m_usersPage->setUsers(m_users);
}

void MainWindow::refreshSessionsPage()
{
    if (!m_sessionsPage)
        return;

    std::sort(m_sessions.begin(), m_sessions.end(), sessionLess);
    m_sessions.erase(std::unique(m_sessions.begin(), m_sessions.end(), sessionsEqual), m_sessions.end());

    QHash<QString, QString> nameMap;
    for (const auto& user : m_users)
        nameMap.insert(user.account, user.name);

    m_sessionsPage->setSessions(m_sessions, nameMap);
}

void MainWindow::refreshBillingSummary()
{
    if (!m_billingPage)
        return;

    if (!m_hasComputed || m_latestBills.empty())
    {
        m_billingPage->setSummary(0, 0.0, static_cast<int>(m_users.size()));
        return;
    }

    int totalMinutes = 0;
    double totalAmount = 0.0;
    for (const auto& line : m_latestBills)
    {
        totalMinutes += line.minutes;
        totalAmount += line.amount;
    }
    m_billingPage->setSummary(totalMinutes, totalAmount, static_cast<int>(m_latestBills.size()));
}

void MainWindow::resetComputedBills()
{
    m_hasComputed = false;
    m_lastBillYear = 0;
    m_lastBillMonth = 0;
    m_latestBills.clear();

    if (m_billingPage)
    {
        m_billingPage->setBillLines({});
    }
    refreshBillingSummary();

    if (m_reportsPage)
    {
        QVector<int> buckets(4, 0);
        const QDate today = QDate::currentDate();
        m_reportsPage->setMonthlySummary(today.year(), today.month(), buckets, 0.0);
    }
}

void MainWindow::handleCreateUser()
{
    UserEditorDialog dialog(UserEditorDialog::Mode::Create, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    User user = dialog.user();
    const auto exists = std::any_of(m_users.begin(), m_users.end(), [&](const User& u) {
        return u.account.compare(user.account, Qt::CaseInsensitive) == 0;
    });
    if (exists)
    {
        QMessageBox::warning(this, windowTitle(), QStringLiteral(u8"账号已存在，请使用唯一编号。"));
        return;
    }

    m_users.push_back(std::move(user));
    m_usersDirty = true;
    refreshUsersPage();
    refreshSessionsPage();
    resetComputedBills();
}

void MainWindow::handleEditUser(const QString& account)
{
    auto it = std::find_if(m_users.begin(), m_users.end(), [&](const User& u) {
        return u.account == account;
    });
    if (it == m_users.end())
    {
        QMessageBox::warning(this, windowTitle(), QStringLiteral(u8"未找到选中的账号。"));
        return;
    }

    UserEditorDialog dialog(UserEditorDialog::Mode::Edit, this);
    dialog.setUser(*it);
    if (dialog.exec() != QDialog::Accepted)
        return;

    User updated = dialog.user();
    it->name = updated.name;
    it->plan = updated.plan;
    m_usersDirty = true;
    refreshUsersPage();
    refreshSessionsPage();
    resetComputedBills();
}

void MainWindow::handleDeleteUsers(const QStringList& accounts)
{
    if (accounts.isEmpty())
        return;

    if (QMessageBox::question(this, windowTitle(),
                              QStringLiteral(u8"确定要删除选中的 %1 个用户吗？\n关联的上网记录也会一并移除。").arg(accounts.size()))
        != QMessageBox::Yes)
    {
        return;
    }

    QSet<QString> target;
    for (const auto& acc : accounts)
        target.insert(acc);

    const auto oldUserSize = m_users.size();
    m_users.erase(std::remove_if(m_users.begin(), m_users.end(), [&](const User& u) {
                     return target.contains(u.account);
                 }),
                 m_users.end());

    const auto oldSessionSize = m_sessions.size();
    m_sessions.erase(std::remove_if(m_sessions.begin(), m_sessions.end(), [&](const Session& s) {
                        return target.contains(s.account);
                    }),
                    m_sessions.end());

    if (m_users.size() != oldUserSize)
        m_usersDirty = true;
    if (m_sessions.size() != oldSessionSize)
        m_sessionsDirty = true;

    refreshUsersPage();
    refreshSessionsPage();
    resetComputedBills();
}

void MainWindow::handleReloadUsers()
{
    if (m_usersDirty)
    {
        if (QMessageBox::question(this, windowTitle(), QStringLiteral(u8"有未保存的用户信息，确认要放弃修改并重新加载吗？")) != QMessageBox::Yes)
            return;
    }

    if (!m_repository)
        return;

    m_users = m_repository->loadUsers();
    m_usersDirty = false;
    refreshUsersPage();
    refreshSessionsPage();
    resetComputedBills();
}

void MainWindow::handleSaveUsers()
{
    if (!m_repository)
        return;

    if (m_repository->saveUsers(m_users))
    {
        m_usersDirty = false;
        QMessageBox::information(this, windowTitle(), QStringLiteral(u8"用户信息已保存。"));
    }
    else
    {
        QMessageBox::warning(this, windowTitle(), QStringLiteral(u8"保存用户信息失败。"));
    }
}

void MainWindow::handleCreateSession()
{
    if (m_users.empty())
    {
        QMessageBox::warning(this, windowTitle(), QStringLiteral(u8"请先创建用户后再录入上网记录。"));
        return;
    }

    QHash<QString, QString> names;
    for (const auto& user : m_users)
        names.insert(user.account, user.name);

    SessionEditorDialog dialog(names, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    Session session = dialog.session();
    if (!names.contains(session.account))
    {
        QMessageBox::warning(this, windowTitle(), QStringLiteral(u8"账号不存在，请在用户管理中新增该账号。"));
        return;
    }
    if (session.end < session.begin)
    {
        QMessageBox::warning(this, windowTitle(), QStringLiteral(u8"结束时间不能早于开始时间。"));
        return;
    }

    m_sessions.push_back(std::move(session));
    m_sessionsDirty = true;
    refreshSessionsPage();
    resetComputedBills();
}

void MainWindow::handleEditSession(const Session& session)
{
    auto it = std::find_if(m_sessions.begin(), m_sessions.end(), [&](const Session& s) {
        return sessionsEqual(s, session);
    });
    if (it == m_sessions.end())
    {
        QMessageBox::warning(this, windowTitle(), QStringLiteral(u8"未找到选中的上网记录。"));
        return;
    }

    QHash<QString, QString> names;
    for (const auto& user : m_users)
        names.insert(user.account, user.name);

    SessionEditorDialog dialog(names, this);
    dialog.setSession(*it);
    if (dialog.exec() != QDialog::Accepted)
        return;

    Session updated = dialog.session();
    if (!names.contains(updated.account))
    {
        QMessageBox::warning(this, windowTitle(), QStringLiteral(u8"账号不存在，请在用户管理中维护该账号。"));
        return;
    }
    if (updated.end < updated.begin)
    {
        QMessageBox::warning(this, windowTitle(), QStringLiteral(u8"结束时间不能早于开始时间。"));
        return;
    }

    *it = std::move(updated);
    m_sessionsDirty = true;
    refreshSessionsPage();
    resetComputedBills();
}

void MainWindow::handleDeleteSessions(const QList<Session>& sessions)
{
    if (sessions.isEmpty())
        return;

    if (QMessageBox::question(this, windowTitle(),
                              QStringLiteral(u8"确定要删除选中的 %1 条上网记录吗？").arg(sessions.size()))
        != QMessageBox::Yes)
    {
        return;
    }

    std::vector<Session> targets(sessions.begin(), sessions.end());
    const auto oldSize = m_sessions.size();
    m_sessions.erase(std::remove_if(m_sessions.begin(), m_sessions.end(), [&](const Session& s) {
                        return std::any_of(targets.begin(), targets.end(), [&](const Session& t) {
                            return sessionsEqual(s, t);
                        });
                    }),
                    m_sessions.end());

    if (m_sessions.size() != oldSize)
    {
        m_sessionsDirty = true;
        refreshSessionsPage();
        resetComputedBills();
    }
}

void MainWindow::handleReloadSessions()
{
    if (m_sessionsDirty)
    {
        if (QMessageBox::question(this, windowTitle(), QStringLiteral(u8"有未保存的上网记录，确认要放弃修改并重新加载吗？")) != QMessageBox::Yes)
            return;
    }

    if (!m_repository)
        return;

    m_sessions = m_repository->loadSessions();
    m_sessionsDirty = false;
    refreshSessionsPage();
    resetComputedBills();
}

void MainWindow::handleSaveSessions()
{
    if (!m_repository)
        return;

    if (m_repository->saveSessions(m_sessions))
    {
        m_sessionsDirty = false;
        QMessageBox::information(this, windowTitle(), QStringLiteral(u8"上网记录已保存。"));
    }
    else
    {
        QMessageBox::warning(this, windowTitle(), QStringLiteral(u8"保存上网记录失败。"));
    }
}

void MainWindow::handleGenerateRandomSessions()
{
    if (m_users.empty())
    {
        QMessageBox::warning(this, windowTitle(), QStringLiteral(u8"请先创建至少一个用户。"));
        return;
    }

    QRandomGenerator* rng = QRandomGenerator::global();
    const QDate baseDate = QDate::currentDate().addMonths(-2).addDays(1 - QDate::currentDate().day());
    const int daySpan = 120;

    for (const auto& user : m_users)
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

    m_sessionsDirty = true;
    refreshSessionsPage();
    resetComputedBills();
    QMessageBox::information(this, windowTitle(), QStringLiteral(u8"已追加随机生成的上网记录，请检查并保存。"));
}

QString MainWindow::defaultOutputDir() const
{
    return QDir::current().filePath(QStringLiteral("out"));
}

void MainWindow::ensureOutputDir()
{
    if (m_outputDir.isEmpty())
        return;
    QDir dir(m_outputDir);
    if (!dir.exists())
        QDir().mkpath(m_outputDir);
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

    int totalMinutes = 0;
    double totalAmount = 0.0;
    QVector<int> buckets(4, 0);
    for (const auto& line : m_latestBills)
    {
        totalMinutes += line.minutes;
        totalAmount += line.amount;

        if (line.minutes <= 30 * 60)
            ++buckets[0];
        else if (line.minutes <= 60 * 60)
            ++buckets[1];
        else if (line.minutes <= 150 * 60)
            ++buckets[2];
        else
            ++buckets[3];
    }

    if (m_billingPage)
    {
        m_billingPage->setBillLines(m_latestBills);
        m_billingPage->setSummary(totalMinutes, totalAmount, static_cast<int>(m_latestBills.size()));
    }

    if (m_reportsPage)
        m_reportsPage->setMonthlySummary(year, month, buckets, totalAmount);

    QMessageBox::information(this, windowTitle(), QStringLiteral(u8"账单已生成，当前结果尚未导出。"));
}

void MainWindow::handleExportBilling()
{
    if (!m_repository || !m_hasComputed || m_latestBills.empty())
    {
        QMessageBox::information(this, windowTitle(), QStringLiteral(u8"请先生成账单后再导出。"));
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
                                 QStringLiteral(u8"账单已导出到：\n%1").arg(QDir::toNativeSeparators(fileName)));
    }
    else
    {
        QMessageBox::warning(this, windowTitle(), QStringLiteral(u8"写入账单文件失败，请检查目录权限。"));
    }
}
