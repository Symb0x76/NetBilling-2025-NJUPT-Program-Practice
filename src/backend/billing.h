#pragma once
#include "Models.h"
#include <unordered_map>

class BillingEngine
{
public:
    static std::vector<BillLine> computeMonthly(
        int year, int month,
        const std::vector<User> &users,
        const std::vector<Session> &sessions);

private:
    static int minutesInMonthPortion(const QDateTime &b, const QDateTime &e, int year, int month);
    static double pricePerMinute() { return 0.03; }
    static int includedMinutes(Tariff t);
    static double baseFee(Tariff t);
};