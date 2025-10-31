#pragma once

#include <QDateTime>
#include <QMetaType>
#include <QString>
#include <vector>

enum class Tariff : int
{
    NoDiscount = 0, // 0.03 元/分钟
    Pack30h = 1,    // 50 元包 30 小时，超出部分 0.03/分钟
    Pack60h = 2,    // 95 元包 60 小时，超出部分 0.03/分钟
    Pack150h = 3,   // 200 元包 150 小时，超出部分 0.03/分钟
    Unlimited = 4   // 300 元不限时
};

inline QString tariffBillingDescription(Tariff plan)
{
    switch (plan)
    {
    case Tariff::NoDiscount:
        return QStringLiteral(u"无优惠：按每分钟 0.03 元计费。");
    case Tariff::Pack30h:
        return QStringLiteral(u"每月 50 元含 30 小时，超出部分按每分钟 0.03 元计费。");
    case Tariff::Pack60h:
        return QStringLiteral(u"每月 95 元含 60 小时，超出部分按每分钟 0.03 元计费。");
    case Tariff::Pack150h:
        return QStringLiteral(u"每月 200 元含 150 小时，超出部分按每分钟 0.03 元计费。");
    case Tariff::Unlimited:
        return QStringLiteral(u"每月 300 元，上网时长不限。");
    }
    return QStringLiteral(u"套餐计费方式未知。");
}

enum class UserRole : int
{
    Admin = 0,
    User = 1
};

struct User
{
    QString name;                  // 姓名
    QString account;               // 账号（唯一）
    Tariff plan;                   // 套餐类型
    QString passwordHash;          // 登录密码哈希
    UserRole role{UserRole::User}; // 角色
    bool enabled{true};            // 是否启用
    double balance{0.0};           // 当前余额
};

struct Session
{
    QString account;
    QDateTime begin;
    QDateTime end;
};

struct BillLine
{
    QString account;
    QString name;
    int plan;
    int minutes;
    double amount;
};

struct RechargeRecord
{
    QString account;
    QDateTime timestamp;
    double amount;
    QString operatorAccount;
    QString note;
    double balanceAfter;
};

Q_DECLARE_METATYPE(User)
Q_DECLARE_METATYPE(Session)
Q_DECLARE_METATYPE(BillLine)
Q_DECLARE_METATYPE(UserRole)
Q_DECLARE_METATYPE(RechargeRecord)
