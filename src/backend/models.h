#pragma once
#include <QString>
#include <QMetaType>
#include <QDateTime>
#include <vector>

enum class Tariff : int
{
    NoDiscount = 0, // 0.03 元/分钟
    Pack30h = 1,    // 50 元包 30 小时，超出 0.03/分钟
    Pack60h = 2,    // 95 元包 60 小时，超出 0.03/分钟
    Pack150h = 3,   // 200 元包 150 小时，超出 0.03/分钟
    Unlimited = 4   // 300 元不限时
};

struct User // 用户信息
{
    QString name;    // 姓名
    QString account; // 账号
    Tariff plan;     // 套餐
};

struct Session // 会话
{
    QString account; // 账号
    QDateTime begin; // 开始时间
    QDateTime end;   // 结束时间
};

struct BillLine // 账单明细
{
    QString account; // 账号
    QString name;    // 姓名
    int plan;        // 套餐代码
    int minutes;     // 当月合计分钟
    double amount;   // 应缴金额
};
Q_DECLARE_METATYPE(User)
Q_DECLARE_METATYPE(Session)
Q_DECLARE_METATYPE(BillLine)
