#pragma once

#include <QString>

namespace Security
{
QString hashPassword(const QString &plainText);
bool verifyPassword(const QString &plainText, const QString &hashed);
QString generateRandomPassword(int length = 10);
}
