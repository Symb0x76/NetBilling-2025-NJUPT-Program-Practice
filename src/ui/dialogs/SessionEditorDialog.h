#pragma once

#include "ElaDialog.h"

#include "backend/Models.h"

#include <QDate>
#include <QHash>
#include <QStringList>

class ElaComboBox;
class ElaPushButton;
class QTimeEdit;
class QWidget;

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
    struct DateTimeControls
    {
        ElaComboBox *yearCombo{nullptr};
        ElaComboBox *monthCombo{nullptr};
        QTimeEdit *timeEdit{nullptr};
        int storedDay{QDate::currentDate().day()};
    };

    void setupUi();
    QString validate() const;
    QWidget *createDateSelector(DateTimeControls &controls);
    void populateYearCombo(ElaComboBox *combo) const;
    void populateMonthCombo(ElaComboBox *combo) const;
    int ensureYearOption(ElaComboBox *combo, int year) const;
    void setDate(DateTimeControls &controls, const QDate &date);
    QDate extractDate(const DateTimeControls &controls) const;

    QHash<QString, QString> m_accountNames;
    ElaComboBox *m_accountCombo{nullptr};
    DateTimeControls m_beginControls;
    DateTimeControls m_endControls;
};
