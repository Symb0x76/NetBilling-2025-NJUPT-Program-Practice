#pragma once

#include "ui/pages/base_page.h"

#include "backend/models.h"

#include <QHash>

#include <memory>
#include <vector>

class ElaComboBox;
class ElaLineEdit;
class ElaPushButton;
class ElaTableView;
class ElaText;
class QDoubleSpinBox;
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
    QString selectedAccount() const;

    bool m_adminMode{false};
    QString m_currentAccount;
    ElaComboBox *m_accountCombo{nullptr};
    QDoubleSpinBox *m_amountSpin{nullptr};
    ElaLineEdit *m_noteEdit{nullptr};
    ElaPushButton *m_rechargeButton{nullptr};
    ElaText *m_balanceLabel{nullptr};
    ElaTableView *m_table{nullptr};
    std::unique_ptr<QStandardItemModel> m_model;
    QHash<QString, QString> m_accountNames;
};
