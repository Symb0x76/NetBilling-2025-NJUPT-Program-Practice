#include "repository.h"
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QStringConverter>

static QDateTime parseCompact(const QString &s)
{
    // 形如：yyyyMMddHHmmss
    return QDateTime::fromString(s, "yyyyMMddHHmmss");
}
static QString formatCompact(const QDateTime &dt)
{
    return dt.toString("yyyyMMddHHmmss");
}

Repository::Repository(QString dataDir, QString outDir)
    : m_dataDir(std::move(dataDir)), m_outDir(std::move(outDir)) {}

std::vector<User> Repository::loadUsers() const
{
    std::vector<User> v;
    QFile f(usersPath());
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return v;
    QTextStream in(&f);
    in.setEncoding(QStringConverter::Utf8);
    while (!in.atEnd())
    {
        QString name;
        QString account;
        int plan;
        in >> name;
        if (name.isEmpty())
            break;
        in >> account >> plan;
        v.push_back(User{name, account, static_cast<Tariff>(plan)});
        in.readLine(); // consume rest
    }
    return v;
}

std::vector<Session> Repository::loadSessions() const
{
    std::vector<Session> v;
    QFile f(sessionsPath());
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return v;
    QTextStream in(&f);
    in.setEncoding(QStringConverter::Utf8);
    while (!in.atEnd())
    {
        QString account, s1, s2;
        in >> account >> s1 >> s2;
        if (account.isEmpty())
            break;
        v.push_back(Session{account, parseCompact(s1), parseCompact(s2)});
        in.readLine();
    }
    return v;
}

bool Repository::saveUsers(const std::vector<User> &users) const
{
    QDir().mkpath(m_dataDir);
    QFile f(usersPath());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    QTextStream out(&f);
    out.setEncoding(QStringConverter::Utf8);
    for (auto &u : users)
        out << u.name << ' ' << u.account << ' ' << static_cast<int>(u.plan) << "\n";
    return true;
}

bool Repository::saveSessions(const std::vector<Session> &ss) const
{
    QDir().mkpath(m_dataDir);
    QFile f(sessionsPath());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    QTextStream out(&f);
    out.setEncoding(QStringConverter::Utf8);
    for (auto &s : ss)
        out << s.account << ' ' << formatCompact(s.begin) << ' ' << formatCompact(s.end) << "\n";
    return true;
}

bool Repository::writeMonthlyBill(int year, int month, const std::vector<BillLine> &lines) const
{
    QDir().mkpath(m_outDir);
    QString fname = QString("%1/bill_%2_%3.txt").arg(m_outDir).arg(year).arg(month, 2, 10, QLatin1Char('0'));
    QFile f(fname);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    QTextStream out(&f);
    out.setEncoding(QStringConverter::Utf8);
    for (auto &b : lines)
    {
        out << b.account << ' ' << b.name << ' ' << b.plan << ' '
            << b.minutes << ' ' << QString::number(b.amount, 'f', 2) << "\n";
    }
    return true;
}

QString Repository::usersPath() const { return m_dataDir + "/users.txt"; }
QString Repository::sessionsPath() const { return m_dataDir + "/sessions.txt"; }
QString Repository::outputDir() const { return m_outDir; }