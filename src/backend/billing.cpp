#include "billing.h"
#include <QDate>

static QDateTime clampBegin(int y, int m, const QDateTime &dt)
{
    QDateTime s(QDate(y, m, 1), QTime(0, 0, 0));
    return dt > s ? dt : s;
}
static QDateTime clampEnd(int y, int m, const QDateTime &dt)
{
    QDate last = QDate(y, m, 1).addMonths(1).addDays(-1);
    QDateTime e(QDateTime(QDate(last.year(), last.month(), last.day()), QTime(23, 59, 59)));
    return dt < e ? dt : e;
}

int BillingEngine::minutesInMonthPortion(const QDateTime &b, const QDateTime &e, int y, int m)
{
    // 计算该会话在指定年月内的秒数并向上取整到分钟
    auto cb = clampBegin(y, m, b);
    auto ce = clampEnd(y, m, e);
    if (ce < cb)
        return 0;

    // 取月边界 [y-m-1 00:00:00, y-m-last 24:00:00)
    QDateTime start = cb;
    QDateTime end = ce;
    // 如果会话跨到下月，截止在当月末 24:00:00
    QDate monthEndDate = QDate(y, m, 1).addMonths(1); // 下月一号 00:00
    QDateTime hardEnd(monthEndDate, QTime(0, 0, 0));
    if (end >= hardEnd)
        end = hardEnd;

    qint64 secs = start.secsTo(end);
    if (secs <= 0)
        return 0;
    int mins = static_cast<int>((secs + 59) / 60); // 不足一分钟按一分钟
    return mins;
}

int BillingEngine::includedMinutes(Tariff t)
{
    switch (t)
    {
    case Tariff::Pack30h:
        return 30 * 60;
    case Tariff::Pack60h:
        return 60 * 60;
    case Tariff::Pack150h:
        return 150 * 60;
    default:
        return 0;
    }
}
double BillingEngine::baseFee(Tariff t)
{
    switch (t)
    {
    case Tariff::NoDiscount:
        return 0.0;
    case Tariff::Pack30h:
        return 50.0;
    case Tariff::Pack60h:
        return 95.0;
    case Tariff::Pack150h:
        return 200.0;
    case Tariff::Unlimited:
        return 300.0;
    }
    return 0.0;
}

std::vector<BillLine> BillingEngine::computeMonthly(
    int year, int month,
    const std::vector<User> &users,
    const std::vector<Session> &sessions)
{
    std::unordered_map<QString, BillLine> map;
    std::unordered_map<QString, Tariff> planOf;

    for (auto &u : users)
    {
        planOf[u.account] = u.plan;
        map[u.account] = BillLine{u.account, u.name, static_cast<int>(u.plan), 0, 0.0};
    }

    // 汇总分钟
    for (auto &s : sessions)
    {
        auto it = planOf.find(s.account);
        if (it == planOf.end())
            continue;
        int mins = minutesInMonthPortion(s.begin, s.end, year, month);
        if (mins > 0)
            map[s.account].minutes += mins;
    }

    // 计费
    std::vector<BillLine> out;
    out.reserve(map.size());
    for (auto &[acc, bl] : map)
    {
        Tariff t = static_cast<Tariff>(bl.plan);
        double fee = 0.0;
        if (t == Tariff::NoDiscount)
        {
            fee = bl.minutes * pricePerMinute();
        }
        else if (t == Tariff::Unlimited)
        {
            fee = baseFee(t);
        }
        else
        {
            fee = baseFee(t);
            int inc = includedMinutes(t);
            if (bl.minutes > inc)
                fee += (bl.minutes - inc) * pricePerMinute();
        }
        bl.amount = std::max(0.0, fee);
        out.push_back(bl);
    }
    // 可按账号排序
    std::sort(out.begin(), out.end(), [](const BillLine &a, const BillLine &b)
              { return a.account < b.account; });
    return out;
}