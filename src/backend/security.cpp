#include "backend/Security.h"

#include <QCryptographicHash>
#include <QRandomGenerator>

namespace
{
    constexpr auto kSalt = "NetBilling::PasswordSalt";

    QByteArray salted(const QString &plain)
    {
        QByteArray data = plain.toUtf8();
        data.append(kSalt, sizeof(kSalt) - 1);
        return data;
    }
} // namespace

namespace Security
{
    QString hashPassword(const QString &plainText)
    {
        const QByteArray digest = QCryptographicHash::hash(salted(plainText), QCryptographicHash::Sha256);
        return QString::fromLatin1(digest.toHex());
    }

    bool verifyPassword(const QString &plainText, const QString &hashed)
    {
        return hashPassword(plainText) == hashed;
    }

    QString generateRandomPassword(int length)
    {
        static const QString characters = QStringLiteral("ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnpqrstuvwxyz23456789");
        QString result;
        result.reserve(length);
        auto *rng = QRandomGenerator::global();
        for (int i = 0; i < length; ++i)
        {
            const int index = rng->bounded(characters.size());
            result.append(characters[index]);
        }
        return result;
    }
} // namespace Security
