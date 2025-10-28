#pragma once
#include "models.h"
#include <optional>

class Repository
{
public:
    explicit Repository(QString dataDir, QString outDir);

    std::vector<User> loadUsers() const;
    std::vector<Session> loadSessions() const;

    bool saveUsers(const std::vector<User> &users) const;
    bool saveSessions(const std::vector<Session> &sessions) const;

    bool writeMonthlyBill(int year, int month, const std::vector<BillLine> &lines) const;

    QString usersPath() const;
    QString sessionsPath() const;
    QString outputDir() const;

private:
    QString m_dataDir;
    QString m_outDir;
};