#pragma once

#include "ElaDialog.h"

#include "backend/Models.h"

class ElaLineEdit;
class ElaComboBox;
class ElaPushButton;
class ElaCheckBox;
class QDoubleValidator;

class UserEditorDialog : public ElaDialog
{
    Q_OBJECT

public:
    enum class Mode
    {
        Create,
        Edit
    };

    explicit UserEditorDialog(Mode mode, QWidget *parent = nullptr);

    void setUser(const User &user);
    User user() const;

protected:
    void accept() override;

private:
    void setupUi();
    QString validate() const;

    Mode m_mode;
    ElaLineEdit *m_nameEdit{nullptr};
    ElaLineEdit *m_accountEdit{nullptr};
    ElaComboBox *m_planCombo{nullptr};
    ElaComboBox *m_roleCombo{nullptr};
    ElaCheckBox *m_enabledCheck{nullptr};
    ElaLineEdit *m_balanceEdit{nullptr};
    QDoubleValidator *m_balanceValidator{nullptr};
    ElaLineEdit *m_passwordEdit{nullptr};
    ElaLineEdit *m_confirmPasswordEdit{nullptr};
    QString m_originalPasswordHash;
};
