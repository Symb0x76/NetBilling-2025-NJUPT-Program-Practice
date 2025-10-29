#pragma once

#include "ElaDialog.h"

#include "backend/Models.h"

#include <QHash>
#include <QStringList>

class ElaComboBox;
class QDateTimeEdit;
class ElaPushButton;

class SessionEditorDialog : public ElaDialog
{
    Q_OBJECT

public:
    explicit SessionEditorDialog(const QHash<QString, QString> &accountNames, QWidget *parent = nullptr);

    void setSession(const Session &session);
    Session session() const;

protected:
    void accept() override;

private:
    void setupUi();
    QString validate() const;

    QHash<QString, QString> m_accountNames;
    ElaComboBox *m_accountCombo{nullptr};
    QDateTimeEdit *m_beginEdit{nullptr};
    QDateTimeEdit *m_endEdit{nullptr};
};
