#pragma once

#include "ElaDialog.h"

#include <QString>

class ElaLineEdit;

class ChangePasswordDialog : public ElaDialog
{
    Q_OBJECT

public:
    explicit ChangePasswordDialog(QWidget *parent = nullptr);

    QString account() const;
    QString oldPassword() const;
    QString newPassword() const;
    void setAccount(const QString &account);

protected:
    void accept() override;

private:
    void setupUi();
    QString validate() const;

    ElaLineEdit *m_accountEdit{nullptr};
    ElaLineEdit *m_oldPasswordEdit{nullptr};
    ElaLineEdit *m_newPasswordEdit{nullptr};
    ElaLineEdit *m_confirmPasswordEdit{nullptr};
    QString m_prefillAccount;
};
