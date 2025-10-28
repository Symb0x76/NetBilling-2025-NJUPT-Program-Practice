#pragma once

#include "ElaDialog.h"

#include "backend/models.h"

class ElaLineEdit;
class ElaComboBox;
class ElaPushButton;

class UserEditorDialog : public ElaDialog
{
    Q_OBJECT

public:
    enum class Mode
    {
        Create,
        Edit
    };

    explicit UserEditorDialog(Mode mode, QWidget* parent = nullptr);

    void setUser(const User& user);
    User user() const;

protected:
    void accept() override;

private:
    void setupUi();
    QString validate() const;

    Mode m_mode;
    ElaLineEdit* m_nameEdit{nullptr};
    ElaLineEdit* m_accountEdit{nullptr};
    ElaComboBox* m_planCombo{nullptr};
};
