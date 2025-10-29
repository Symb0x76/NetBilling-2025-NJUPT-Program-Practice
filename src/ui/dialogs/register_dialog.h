#pragma once

#include "ElaDialog.h"

#include "backend/models.h"

class ElaLineEdit;
class ElaComboBox;

class RegistrationDialog : public ElaDialog
{
    Q_OBJECT

public:
    explicit RegistrationDialog(QWidget *parent = nullptr);

    User user() const;

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
};
