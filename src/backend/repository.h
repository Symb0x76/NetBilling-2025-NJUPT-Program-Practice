#pragma once

#include "Models.h"

class Repository
{
public:
    explicit Repository(QString dataDir, QString outDir);

    std::vector<User> loadUsers() const;
    std::vector<Session> loadSessions() const;
    std::vector<RechargeRecord> loadRechargeRecords() const;

    bool saveUsers(const std::vector<User> &users) const;
    bool saveSessions(const std::vector<Session> &sessions) const;
    bool saveRechargeRecords(const std::vector<RechargeRecord> &records) const;
    bool appendRechargeRecord(const RechargeRecord &record) const;

    bool writeMonthlyBill(int year, int month, const std::vector<BillLine> &lines) const;

    QString usersPath() const;
    QString sessionsPath() const;
    QString billsPath() const;
    QString outputDir() const;

private:
    QString m_dataDir;
    QString m_outDir;
};
