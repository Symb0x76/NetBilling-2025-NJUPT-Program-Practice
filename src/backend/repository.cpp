#include "repository.h"

#include "backend/security.h"

#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QStringConverter>
#include <QTextStream>

namespace
{
Tariff toTariff(int value)
{
    if (value < static_cast<int>(Tariff::NoDiscount) || value > static_cast<int>(Tariff::Unlimited))
        return Tariff::NoDiscount;
    return static_cast<Tariff>(value);
}

QString boolToFlag(bool value)
{
    return value ? QStringLiteral("1") : QStringLiteral("0");
}

bool flagToBool(const QString &value)
{
    return value.trimmed() == QLatin1String("1") || value.trimmed().compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0;
}

QDateTime parseCompact(const QString &s)
{
    // 例如 yyyyMMddHHmmss
    return QDateTime::fromString(s, QStringLiteral("yyyyMMddHHmmss"));
}

QString formatCompact(const QDateTime &dt)
{
    return dt.toString(QStringLiteral("yyyyMMddHHmmss"));
}
} // namespace

Repository::Repository(QString dataDir, QString outDir)
    : m_dataDir(std::move(dataDir)), m_outDir(std::move(outDir))
{
}

std::vector<User> Repository::loadUsers() const
{
    std::vector<User> users;
    QFile file(usersPath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return users;

    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);
    QString line;
    while (in.readLineInto(&line))
    {
        line = line.trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;

        User user;
        if (line.contains(QLatin1Char('|')))
        {
            const QStringList parts = line.split(QLatin1Char('|'));
            user.account = parts.value(0).trimmed();
            user.name = parts.value(1).trimmed();
            user.plan = toTariff(parts.value(2).toInt());
            user.passwordHash = parts.value(3).trimmed();
            user.role = static_cast<UserRole>(parts.value(4, QStringLiteral("1")).toInt());
            user.enabled = flagToBool(parts.value(5, QStringLiteral("1")));
            user.balance = parts.value(6, QStringLiteral("0")).toDouble();
        }
        else
        {
            const QStringList tokens = line.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
            if (tokens.size() < 3)
                continue;
            user.name = tokens.value(0);
            user.account = tokens.value(1);
            user.plan = toTariff(tokens.value(2).toInt());
            user.passwordHash = Security::hashPassword(QStringLiteral("123456"));
            user.role = UserRole::User;
            user.enabled = true;
            user.balance = 0.0;
        }

        if (user.account.isEmpty())
            continue;

        if (user.passwordHash.isEmpty())
            user.passwordHash = Security::hashPassword(QStringLiteral("123456"));

        if (user.role != UserRole::Admin && user.role != UserRole::User)
            user.role = UserRole::User;

        users.push_back(std::move(user));
    }
    return users;
}

std::vector<Session> Repository::loadSessions() const
{
    std::vector<Session> sessions;
    QFile file(sessionsPath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return sessions;

    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);
    while (!in.atEnd())
    {
        QString account, beginStr, endStr;
        in >> account >> beginStr >> endStr;
        if (account.isEmpty())
            break;
        sessions.push_back(Session{account, parseCompact(beginStr), parseCompact(endStr)});
        in.readLine(); // consume the rest
    }
    return sessions;
}

bool Repository::saveUsers(const std::vector<User> &users) const
{
    QDir().mkpath(m_dataDir);
    QFile file(usersPath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    for (const auto &user : users)
    {
        out << user.account << '|'
            << user.name << '|'
            << static_cast<int>(user.plan) << '|'
            << user.passwordHash << '|'
            << static_cast<int>(user.role) << '|'
            << boolToFlag(user.enabled) << '|'
            << QString::number(user.balance, 'f', 2) << '\n';
    }
    return true;
}

bool Repository::saveSessions(const std::vector<Session> &sessions) const
{
    QDir().mkpath(m_dataDir);
    QFile file(sessionsPath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    for (const auto &session : sessions)
    {
        out << session.account << ' '
            << formatCompact(session.begin) << ' '
            << formatCompact(session.end) << '\n';
    }
    return true;
}

bool Repository::writeMonthlyBill(int year, int month, const std::vector<BillLine> &lines) const
{
    QDir().mkpath(m_outDir);
    const QString fileName = QStringLiteral("%1/bill_%2_%3.txt").arg(m_outDir).arg(year).arg(month, 2, 10, QLatin1Char('0'));
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    for (const auto &line : lines)
    {
        out << line.account << ' '
            << line.name << ' '
            << line.plan << ' '
            << line.minutes << ' '
            << QString::number(line.amount, 'f', 2) << '\n';
    }
    return true;
}

std::vector<RechargeRecord> Repository::loadRechargeRecords() const
{
    std::vector<RechargeRecord> records;
    QFile file(rechargesPath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return records;

    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);
    QString line;
    while (in.readLineInto(&line))
    {
        line = line.trimmed();
        if (line.isEmpty())
            continue;
        const QStringList parts = line.split(QLatin1Char('|'));
        RechargeRecord record;
        record.account = parts.value(0).trimmed();
        record.timestamp = QDateTime::fromString(parts.value(1).trimmed(), Qt::ISODate);
        record.amount = parts.value(2).toDouble();
        record.operatorAccount = parts.value(3).trimmed();
        record.note = parts.value(4).trimmed();
        record.balanceAfter = parts.value(5).toDouble();
        if (!record.timestamp.isValid())
            record.timestamp = QDateTime::currentDateTime();
        records.push_back(std::move(record));
    }
    return records;
}

bool Repository::saveRechargeRecords(const std::vector<RechargeRecord> &records) const
{
    QDir().mkpath(m_dataDir);
    QFile file(rechargesPath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    for (const auto &record : records)
    {
        out << record.account << '|'
            << record.timestamp.toString(Qt::ISODate) << '|'
            << QString::number(record.amount, 'f', 2) << '|'
            << record.operatorAccount << '|'
            << record.note << '|'
            << QString::number(record.balanceAfter, 'f', 2) << '\n';
    }
    return true;
}

bool Repository::appendRechargeRecord(const RechargeRecord &record) const
{
    QDir().mkpath(m_dataDir);
    QFile file(rechargesPath());
    if (!file.open(QIODevice::Append | QIODevice::Text))
        return false;

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << record.account << '|'
        << record.timestamp.toString(Qt::ISODate) << '|'
        << QString::number(record.amount, 'f', 2) << '|'
        << record.operatorAccount << '|'
        << record.note << '|'
        << QString::number(record.balanceAfter, 'f', 2) << '\n';
    return true;
}

QString Repository::usersPath() const
{
    return m_dataDir + QStringLiteral("/users.txt");
}

QString Repository::sessionsPath() const
{
    return m_dataDir + QStringLiteral("/sessions.txt");
}

QString Repository::rechargesPath() const
{
    return m_dataDir + QStringLiteral("/recharges.txt");
}

QString Repository::outputDir() const
{
    return m_outDir;
}
