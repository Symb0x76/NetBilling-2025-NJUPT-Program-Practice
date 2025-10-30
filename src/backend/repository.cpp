#include "Repository.h"

#include "backend/Security.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStringConverter>
#include <QStringList>
#include <QTextStream>
#include <QCryptographicHash>

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

    QStringList parseCsvLine(const QString &line)
    {
        QStringList fields;
        QString current;
        bool inQuotes = false;
        for (int i = 0; i < line.size(); ++i)
        {
            const QChar ch = line.at(i);
            if (inQuotes)
            {
                if (ch == QLatin1Char('"'))
                {
                    if (i + 1 < line.size() && line.at(i + 1) == QLatin1Char('"'))
                    {
                        current.append(QLatin1Char('"'));
                        ++i;
                    }
                    else
                    {
                        inQuotes = false;
                    }
                }
                else
                {
                    current.append(ch);
                }
            }
            else
            {
                if (ch == QLatin1Char('"'))
                {
                    inQuotes = true;
                }
                else if (ch == QLatin1Char(','))
                {
                    fields.append(current);
                    current.clear();
                }
                else
                {
                    current.append(ch);
                }
            }
        }
        fields.append(current);
        return fields;
    }

    QString encodeCsvField(const QString &field)
    {
        QString result = field;
        bool needsQuotes = result.contains(QLatin1Char(',')) || result.contains(QLatin1Char('\n')) || result.contains(QLatin1Char('\r'));
        if (result.contains(QLatin1Char('"')))
        {
            result.replace(QLatin1Char('"'), QString(2, QLatin1Char('"')));
            needsQuotes = true;
        }
        if (needsQuotes)
            return QStringLiteral("\"%1\"").arg(result);
        return result;
    }

    void writeCsvRow(QTextStream &out, const QStringList &fields)
    {
        QStringList encoded;
        encoded.reserve(fields.size());
        for (const auto &field : fields)
            encoded.append(encodeCsvField(field));
        out << encoded.join(QLatin1Char(',')) << QLatin1Char('\n');
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
        const QStringList csvParts = parseCsvLine(line);
        if (csvParts.size() >= 7)
        {
            if (csvParts.value(0).trimmed().compare(QStringLiteral("account"), Qt::CaseInsensitive) == 0)
                continue;
            user.account = csvParts.value(0).trimmed();
            user.name = csvParts.value(1).trimmed();
            user.plan = toTariff(csvParts.value(2).trimmed().toInt());
            user.passwordHash = csvParts.value(3).trimmed();
            user.role = static_cast<UserRole>(csvParts.value(4, QStringLiteral("1")).trimmed().toInt());
            user.enabled = flagToBool(csvParts.value(5, QStringLiteral("1")));
            user.balance = csvParts.value(6, QStringLiteral("0")).trimmed().toDouble();
        }
        else
        {
            const QStringList tokens = line.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
            if (tokens.size() >= 7)
            {
                user.account = tokens.value(0).trimmed();
                if (user.account.compare(QStringLiteral("account"), Qt::CaseInsensitive) == 0)
                    continue;
                user.name = tokens.value(1).trimmed();
                user.plan = toTariff(tokens.value(2).toInt());
                user.passwordHash = tokens.value(3).trimmed();
                user.role = static_cast<UserRole>(tokens.value(4, QStringLiteral("1")).toInt());
                user.enabled = flagToBool(tokens.value(5, QStringLiteral("1")));
                user.balance = tokens.value(6, QStringLiteral("0")).toDouble();
            }
            else
            {
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
    QString line;
    while (in.readLineInto(&line))
    {
        line = line.trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;
        const QStringList fields = parseCsvLine(line);
        QString account;
        QString beginStr;
        QString endStr;
        if (fields.size() >= 3)
        {
            account = fields.value(0).trimmed();
            beginStr = fields.value(1).trimmed();
            endStr = fields.value(2).trimmed();
            if (account.compare(QStringLiteral("account"), Qt::CaseInsensitive) == 0)
                continue;
        }
        else
        {
            const QStringList tokens = line.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
            if (tokens.size() < 3)
                continue;
            account = tokens.value(0).trimmed();
            if (account.compare(QStringLiteral("account"), Qt::CaseInsensitive) == 0)
                continue;
            beginStr = tokens.value(1).trimmed();
            endStr = tokens.value(2).trimmed();
        }

        if (account.isEmpty())
            continue;
        sessions.push_back(Session{account, parseCompact(beginStr), parseCompact(endStr)});
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
    writeCsvRow(out,
                {QStringLiteral("account"),
                 QStringLiteral("name"),
                 QStringLiteral("plan"),
                 QStringLiteral("password_hash"),
                 QStringLiteral("role"),
                 QStringLiteral("enabled"),
                 QStringLiteral("balance")});
    for (const auto &user : users)
    {
        writeCsvRow(out,
                    {user.account,
                     user.name,
                     QString::number(static_cast<int>(user.plan)),
                     user.passwordHash,
                     QString::number(static_cast<int>(user.role)),
                     boolToFlag(user.enabled),
                     QString::number(user.balance, 'f', 2)});
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
    writeCsvRow(out,
                {QStringLiteral("account"),
                 QStringLiteral("begin"),
                 QStringLiteral("end")});
    for (const auto &session : sessions)
    {
        writeCsvRow(out,
                    {session.account,
                     formatCompact(session.begin),
                     formatCompact(session.end)});
    }
    return true;
}

bool Repository::writeMonthlyBill(int year, int month, const std::vector<BillLine> &lines) const
{
    QDir().mkpath(m_outDir);
    const QString fileName = QStringLiteral("%1/bill_%2_%3.csv").arg(m_outDir).arg(year).arg(month, 2, 10, QLatin1Char('0'));
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    writeCsvRow(out, {QStringLiteral("account"), QStringLiteral("name"), QStringLiteral("plan"), QStringLiteral("minutes"), QStringLiteral("amount")});
    for (const auto &line : lines)
    {
        writeCsvRow(out,
                    {line.account,
                     line.name,
                     QString::number(line.plan),
                     QString::number(line.minutes),
                     QString::number(line.amount, 'f', 2)});
    }
    return true;
}

std::vector<RechargeRecord> Repository::loadRechargeRecords() const
{
    std::vector<RechargeRecord> records;
    QFile file(billsPath());
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
        RechargeRecord record;
        const QStringList parts = parseCsvLine(line);
        if (parts.size() >= 6)
        {
            if (parts.value(0).trimmed().compare(QStringLiteral("account"), Qt::CaseInsensitive) == 0)
                continue;
            record.account = parts.value(0).trimmed();
            record.timestamp = QDateTime::fromString(parts.value(1).trimmed(), Qt::ISODate);
            record.amount = parts.value(2).trimmed().toDouble();
            record.operatorAccount = parts.value(3).trimmed();
            record.note = parts.value(4).trimmed();
            record.balanceAfter = parts.value(5).trimmed().toDouble();
        }
        else
        {
            QStringList tokens = line.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
            if (tokens.size() < 5)
                continue;

            const QString balanceToken = tokens.takeLast();
            record.balanceAfter = balanceToken.toDouble();

            if (tokens.size() < 4)
                continue;

            record.account = tokens.value(0).trimmed();
            if (record.account.compare(QStringLiteral("account"), Qt::CaseInsensitive) == 0)
                continue;
            record.timestamp = QDateTime::fromString(tokens.value(1).trimmed(), Qt::ISODate);
            record.amount = tokens.value(2).toDouble();
            record.operatorAccount = tokens.value(3).trimmed();
            if (tokens.size() > 4)
                record.note = tokens.mid(4).join(QStringLiteral(" ")).trimmed();
            else
                record.note.clear();
        }

        if (!record.timestamp.isValid())
            record.timestamp = QDateTime::currentDateTime();

        records.push_back(std::move(record));
    }
    return records;
}

bool Repository::saveRechargeRecords(const std::vector<RechargeRecord> &records) const
{
    QDir().mkpath(m_dataDir);
    QFile file(billsPath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    writeCsvRow(out,
                {QStringLiteral("account"),
                 QStringLiteral("timestamp"),
                 QStringLiteral("amount"),
                 QStringLiteral("operator"),
                 QStringLiteral("note"),
                 QStringLiteral("balance_after")});
    for (const auto &record : records)
    {
        writeCsvRow(out,
                    {record.account,
                     record.timestamp.toString(Qt::ISODate),
                     QString::number(record.amount, 'f', 2),
                     record.operatorAccount,
                     record.note,
                     QString::number(record.balanceAfter, 'f', 2)});
    }
    return true;
}

bool Repository::appendRechargeRecord(const RechargeRecord &record) const
{
    QDir().mkpath(m_dataDir);
    QFile file(billsPath());
    if (!file.open(QIODevice::Append | QIODevice::Text))
        return false;

    const bool needsHeader = file.size() == 0;

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    if (needsHeader)
    {
        writeCsvRow(out,
                    {QStringLiteral("account"),
                     QStringLiteral("timestamp"),
                     QStringLiteral("amount"),
                     QStringLiteral("operator"),
                     QStringLiteral("note"),
                     QStringLiteral("balance_after")});
    }
    writeCsvRow(out,
                {record.account,
                 record.timestamp.toString(Qt::ISODate),
                 QString::number(record.amount, 'f', 2),
                 record.operatorAccount,
                 record.note,
                 QString::number(record.balanceAfter, 'f', 2)});
    return true;
}

QString Repository::usersPath() const
{
    return m_dataDir + QStringLiteral("/users.csv");
}

QString Repository::sessionsPath() const
{
    return m_dataDir + QStringLiteral("/sessions.csv");
}

QString Repository::billsPath() const
{
    return m_dataDir + QStringLiteral("/bills.csv");
}

QString Repository::outputDir() const
{
    return m_outDir;
}

QString Repository::dataDir() const
{
    return m_dataDir;
}

bool Repository::exportBackup(const QString &filePath, QString *error) const
{
    const auto setError = [&](const QString &msg) {
        if (error)
            *error = msg;
    };

    QDir dir(m_dataDir);
    if (!dir.exists())
    {
        if (!dir.mkpath(QStringLiteral(".")))
        {
            setError(QStringLiteral(u"数据目录不存在且无法创建：%1").arg(QDir::toNativeSeparators(m_dataDir)));
            return false;
        }
    }

    QJsonArray filesArray;
    const QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::NoSymLinks | QDir::Readable);
    for (const QFileInfo &info : entries)
    {
        QFile file(info.absoluteFilePath());
        if (!file.open(QIODevice::ReadOnly))
        {
            setError(QStringLiteral(u"无法读取文件：%1").arg(info.fileName()));
            return false;
        }

        const QByteArray data = file.readAll();
        QJsonObject fileObject;
        fileObject.insert(QStringLiteral("name"), info.fileName());
        fileObject.insert(QStringLiteral("size"), static_cast<qint64>(data.size()));
        fileObject.insert(QStringLiteral("sha256"),
                          QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex()));
        fileObject.insert(QStringLiteral("data"), QString::fromLatin1(data.toBase64()));
        filesArray.append(fileObject);
    }

    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("createdAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    root.insert(QStringLiteral("fileCount"), filesArray.size());
    root.insert(QStringLiteral("files"), filesArray);

    QSaveFile out(filePath);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        setError(QStringLiteral(u"无法写入备份文件：%1").arg(QDir::toNativeSeparators(filePath)));
        return false;
    }

    const QByteArray payload = QJsonDocument(root).toJson(QJsonDocument::Indented);
    if (out.write(payload) != payload.size())
    {
        setError(QStringLiteral(u"写入备份数据失败。"));
        return false;
    }
    if (!out.commit())
    {
        setError(QStringLiteral(u"提交备份文件失败。"));
        return false;
    }
    return true;
}

bool Repository::importBackup(const QString &filePath, QString *error)
{
    const auto setError = [&](const QString &msg) {
        if (error)
            *error = msg;
    };

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        setError(QStringLiteral(u"无法打开备份文件：%1").arg(QDir::toNativeSeparators(filePath)));
        return false;
    }

    const QByteArray raw = file.readAll();
    QJsonParseError parseError{};
    const auto document = QJsonDocument::fromJson(raw, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        setError(QStringLiteral(u"备份文件格式错误：%1").arg(parseError.errorString()));
        return false;
    }

    const QJsonObject root = document.object();
    const QJsonArray files = root.value(QStringLiteral("files")).toArray();

    QDir dir(m_dataDir);
    if (!dir.exists() && !dir.mkpath(QStringLiteral(".")))
    {
        setError(QStringLiteral(u"无法创建数据目录：%1").arg(QDir::toNativeSeparators(m_dataDir)));
        return false;
    }

    for (const QJsonValue &value : files)
    {
        if (!value.isObject())
        {
            setError(QStringLiteral(u"备份文件包含无效条目。"));
            return false;
        }

        const QJsonObject fileObject = value.toObject();
        const QString name = fileObject.value(QStringLiteral("name")).toString();
        if (name.isEmpty())
        {
            setError(QStringLiteral(u"备份文件包含空文件名。"));
            return false;
        }

        const QString encoded = fileObject.value(QStringLiteral("data")).toString();
        QByteArray data = QByteArray::fromBase64(encoded.toLatin1());
        if (!encoded.isEmpty() && data.isEmpty() && fileObject.value(QStringLiteral("size")).toInt() > 0)
        {
            setError(QStringLiteral(u"备份内容损坏：%1").arg(name));
            return false;
        }

        const QString hashValue = fileObject.value(QStringLiteral("sha256")).toString();
        if (!hashValue.isEmpty())
        {
            const QByteArray expected = QByteArray::fromHex(hashValue.toLatin1());
            if (!expected.isEmpty())
            {
                const QByteArray actual = QCryptographicHash::hash(data, QCryptographicHash::Sha256);
                if (actual != expected)
                {
                    setError(QStringLiteral(u"备份校验失败：%1").arg(name));
                    return false;
                }
            }
        }

        const QFileInfo targetInfo(dir.filePath(name));
        if (!QDir().mkpath(targetInfo.path()))
        {
            setError(QStringLiteral(u"无法创建目标目录：%1").arg(targetInfo.path()));
            return false;
        }

        QSaveFile out(targetInfo.absoluteFilePath());
        if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate))
        {
            setError(QStringLiteral(u"无法恢复文件：%1").arg(name));
            return false;
        }
        if (!data.isEmpty() && out.write(data) != data.size())
        {
            setError(QStringLiteral(u"写入文件失败：%1").arg(name));
            return false;
        }
        if (!out.commit())
        {
            setError(QStringLiteral(u"提交文件失败：%1").arg(name));
            return false;
        }
    }

    return true;
}
