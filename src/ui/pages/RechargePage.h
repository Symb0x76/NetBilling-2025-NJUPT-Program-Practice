#pragma once

#include "ui/pages/BasePage.h"

#include "backend/Models.h"

#include <QHash>

#include <memory>
#include <vector>

class ElaComboBox;
class ElaLineEdit;
class ElaPushButton;
class ElaTableView;
class ElaText;
class QDoubleValidator;
class QStandardItemModel;

class RechargePage : public BasePage
{
    Q_OBJECT

public:
    explicit RechargePage(QWidget *parent = nullptr);

    void setAdminMode(bool adminMode);
    void setUsers(const std::vector<User> &users);
    void setRechargeRecords(const std::vector<RechargeRecord> &records);
    void setCurrentAccount(const QString &account);
    void setCurrentBalance(double balance);

Q_SIGNALS:
    void rechargeRequested(const QString &account, double amount, const QString &note, bool selfService);

private:
    void setupToolbar();
    void setupTable();
    void reloadPageData() override;
    QString selectedAccount() const;
    double currentAmount(bool *ok = nullptr) const;
    void setAmount(double amount);

    bool m_adminMode{false};
    QString m_currentAccount;
    ElaComboBox *m_accountCombo{nullptr};
    ElaLineEdit *m_amountEdit{nullptr};
    QDoubleValidator *m_amountValidator{nullptr};
    ElaLineEdit *m_noteEdit{nullptr};
    ElaPushButton *m_rechargeButton{nullptr};
    ElaText *m_balanceLabel{nullptr};
    ElaTableView *m_table{nullptr};
    std::unique_ptr<QStandardItemModel> m_model;
    QHash<QString, QString> m_accountNames;
};
