#pragma once

#include "ElaDialog.h"

#include "backend/Models.h"

#include <QRegularExpression>
#include <QSet>
#include <QStringList>

class ElaLineEdit;
class ElaComboBox;

class RegistrationDialog : public ElaDialog
{
    Q_OBJECT

public:
    explicit RegistrationDialog(QWidget *parent = nullptr);

    User user() const;
    void setExistingAccounts(const QStringList &accounts);

protected:
    void accept() override;

private:
    void setupUi();
    QString validate() const;

    ElaLineEdit *m_nameEdit{nullptr};
    ElaLineEdit *m_accountEdit{nullptr};
    ElaLineEdit *m_passwordEdit{nullptr};
    ElaLineEdit *m_confirmEdit{nullptr};
    ElaComboBox *m_planCombo{nullptr};
    QRegularExpression m_accountPattern{QStringLiteral("^[A-Za-z0-9]*$")};
    QSet<QString> m_existingAccountsLower;
};
