#pragma once

#include "ElaDialog.h"

#include "backend/Models.h"

#include <QDate>
#include <QHash>
#include <QStringList>

class ElaComboBox;
class ElaPushButton;
class ElaCalendarPicker;
class QWidget;
class QTime;

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
    class TimeSelectorWidget;
    struct DateTimeControls
    {
        ElaCalendarPicker *datePicker{nullptr};
        TimeSelectorWidget *timeSelector{nullptr};
    };

    void setupUi();
    QString validate() const;
    QWidget *createDateSelector(DateTimeControls &controls);
    void setDate(DateTimeControls &controls, const QDate &date);
    QDate extractDate(const DateTimeControls &controls) const;
    TimeSelectorWidget *createTimeSelector(const QTime &initialTime);
    void styleCalendarPicker(ElaCalendarPicker *picker) const;

    QHash<QString, QString> m_accountNames;
    ElaComboBox *m_accountCombo{nullptr};
    DateTimeControls m_beginControls;
    DateTimeControls m_endControls;
};
