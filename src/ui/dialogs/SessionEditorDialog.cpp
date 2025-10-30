#include "ui/dialogs/SessionEditorDialog.h"

#include "ElaAppBar.h"
#include "ElaComboBox.h"
#include "ElaPushButton.h"
#include "ElaText.h"
#include "ui/ThemeUtils.h"

#include <algorithm>
#include <QDate>
#include <QDateTime>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QTime>
#include <QTimeEdit>
#include <QWidget>
#include <QVBoxLayout>
#include <QAbstractSpinBox>

SessionEditorDialog::SessionEditorDialog(const QHash<QString, QString> &accountNames, QWidget *parent)
    : ElaDialog(parent), m_accountNames(accountNames)
{
    setWindowTitle(QStringLiteral(u"编辑上网记录"));
    setMinimumSize(320, 320);
    resize(480, 320);
    setWindowButtonFlag(ElaAppBarType::ThemeChangeButtonHint);
    connect(this, &SessionEditorDialog::themeChangeButtonClicked, this, [this]
            { toggleThemeMode(this); });
    setupUi();
}

void SessionEditorDialog::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(18);

    m_accountCombo = new ElaComboBox(this);
    m_accountCombo->setEditable(true);
    for (auto it = m_accountNames.constBegin(); it != m_accountNames.constEnd(); ++it)
    {
        const QString display = it.value().isEmpty()
                                    ? it.key()
                                    : QStringLiteral("%1 (%2)").arg(it.key(), it.value());
        m_accountCombo->addItem(display, it.key());
    }
    auto *accountLayout = new QHBoxLayout();
    accountLayout->setSpacing(12);
    auto *accountLabel = createFormLabel(QStringLiteral(u"账号"), this);
    accountLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    accountLayout->addWidget(accountLabel);
    accountLayout->addWidget(m_accountCombo, 1);
    layout->addLayout(accountLayout);

    const QTime now = QTime::currentTime();

    m_beginControls.timeEdit = createTimeEdit(now);
    m_endControls.timeEdit = createTimeEdit(now.addSecs(3600));

    auto *beginDateSelector = createDateSelector(m_beginControls);
    auto *endDateSelector = createDateSelector(m_endControls);

    auto *pickerLayout = new QFormLayout();
    pickerLayout->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    pickerLayout->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
    pickerLayout->setHorizontalSpacing(12);
    pickerLayout->setVerticalSpacing(10);
    pickerLayout->setContentsMargins(0, 0, 0, 0);

    auto *beginDateLabel = createFormLabel(QStringLiteral(u"开始年月"), this);
    beginDateLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    auto *beginTimeLabel = createFormLabel(QStringLiteral(u"开始时间"), this);
    beginTimeLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    auto *endDateLabel = createFormLabel(QStringLiteral(u"结束年月"), this);
    endDateLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    auto *endTimeLabel = createFormLabel(QStringLiteral(u"结束时间"), this);
    endTimeLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    pickerLayout->addRow(beginDateLabel, beginDateSelector);
    pickerLayout->addRow(beginTimeLabel, m_beginControls.timeEdit);
    pickerLayout->addRow(endDateLabel, endDateSelector);
    pickerLayout->addRow(endTimeLabel, m_endControls.timeEdit);

    layout->addLayout(pickerLayout);

    layout->addStretch();

    auto *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    auto *cancelButton = new ElaPushButton(QStringLiteral(u"取消"), this);
    auto *okButton = new ElaPushButton(QStringLiteral(u"确定"), this);
    buttonLayout->addWidget(cancelButton);
    buttonLayout->addWidget(okButton);
    layout->addLayout(buttonLayout);

    connect(cancelButton, &ElaPushButton::clicked, this, &SessionEditorDialog::reject);
    connect(okButton, &ElaPushButton::clicked, this, &SessionEditorDialog::accept);

    const QDate today = QDate::currentDate();
    setDate(m_beginControls, today);
    setDate(m_endControls, today);
}

void SessionEditorDialog::setSession(const Session &session)
{
    const int index = m_accountCombo->findData(session.account);
    if (index >= 0)
    {
        m_accountCombo->setCurrentIndex(index);
    }
    else
    {
        m_accountCombo->setEditText(session.account);
    }

    if (session.begin.isValid())
    {
        m_beginControls.timeEdit->setTime(session.begin.time());
        setDate(m_beginControls, session.begin.date());
    }

    if (session.end.isValid())
    {
        m_endControls.timeEdit->setTime(session.end.time());
        setDate(m_endControls, session.end.date());
    }
}

Session SessionEditorDialog::session() const
{
    Session s;
    const QString account = m_accountCombo->currentData().toString();
    s.account = account.isEmpty() ? m_accountCombo->currentText().trimmed() : account;
    const QDate beginRaw = extractDate(m_beginControls);
    const QDate endRaw = extractDate(m_endControls);
    const QDate beginDate = beginRaw.isValid() ? beginRaw : QDate::currentDate();
    const QDate endDate = endRaw.isValid() ? endRaw : beginDate;
    const QDateTime begin(beginDate, m_beginControls.timeEdit->time());
    const QDateTime end(endDate, m_endControls.timeEdit->time());
    s.begin = begin;
    s.end = end;
    return s;
}

QString SessionEditorDialog::validate() const
{
    const QString account = m_accountCombo->currentData().toString().isEmpty()
                                ? m_accountCombo->currentText().trimmed()
                                : m_accountCombo->currentData().toString();
    if (account.isEmpty())
        return QStringLiteral(u"账号不能为空。");
    const QDate beginDate = extractDate(m_beginControls);
    const QDate endDate = extractDate(m_endControls);
    if (!beginDate.isValid() || !endDate.isValid())
        return QStringLiteral(u"请选择有效的开始和结束年月。");
    const QDateTime begin(beginDate, m_beginControls.timeEdit->time());
    const QDateTime end(endDate, m_endControls.timeEdit->time());
    if (!begin.isValid() || !end.isValid())
        return QStringLiteral(u"请选择有效的开始和结束年月。");
    if (begin > end)
        return QStringLiteral(u"结束时间必须晚于开始时间。");
    return {};
}

void SessionEditorDialog::accept()
{
    const auto error = validate();
    if (!error.isEmpty())
    {
        QMessageBox::warning(this, windowTitle(), error);
        return;
    }
    ElaDialog::accept();
}

QWidget *SessionEditorDialog::createDateSelector(DateTimeControls &controls)
{
    auto *container = new QWidget(this);
    auto *selectorLayout = new QHBoxLayout(container);
    selectorLayout->setContentsMargins(0, 0, 0, 0);
    selectorLayout->setSpacing(8);

    controls.yearCombo = new ElaComboBox(container);
    controls.monthCombo = new ElaComboBox(container);

    controls.yearCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    controls.monthCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);

    populateYearCombo(controls.yearCombo);
    populateMonthCombo(controls.monthCombo);

    selectorLayout->addWidget(controls.yearCombo);
    selectorLayout->addWidget(controls.monthCombo);

    return container;
}

QTimeEdit *SessionEditorDialog::createTimeEdit(const QTime &initialTime)
{
    auto *edit = new QTimeEdit(initialTime, this);
    edit->setDisplayFormat(QStringLiteral("HH:mm:ss"));
    edit->setMinimumTime(QTime(0, 0, 0));
    edit->setMaximumTime(QTime(23, 59, 59));
    edit->setKeyboardTracking(false);
    edit->setAlignment(Qt::AlignCenter);
    edit->setWrapping(true);
    edit->setButtonSymbols(QAbstractSpinBox::PlusMinus);
    return edit;
}

void SessionEditorDialog::populateYearCombo(ElaComboBox *combo) const
{
    if (!combo)
        return;

    combo->clear();
    constexpr int minYear = 1990;
    constexpr int maxYear = 2100;
    for (int year = minYear; year <= maxYear; ++year)
    {
        combo->addItem(QStringLiteral("%1 年").arg(year), year);
    }
}

void SessionEditorDialog::populateMonthCombo(ElaComboBox *combo) const
{
    if (!combo)
        return;

    combo->clear();
    for (int month = 1; month <= 12; ++month)
    {
        combo->addItem(QStringLiteral("%1 月").arg(month), month);
    }
}

int SessionEditorDialog::ensureYearOption(ElaComboBox *combo, int year) const
{
    if (!combo)
        return -1;

    int index = combo->findData(year);
    if (index >= 0)
        return index;

    const QSignalBlocker blocker(combo);
    int insertPos = 0;
    for (; insertPos < combo->count(); ++insertPos)
    {
        if (combo->itemData(insertPos).toInt() > year)
            break;
    }
    combo->insertItem(insertPos, QStringLiteral("%1 年").arg(year), year);
    return combo->findData(year);
}

void SessionEditorDialog::setDate(DateTimeControls &controls, const QDate &date)
{
    const QDate target = date.isValid() ? date : QDate::currentDate();
    controls.storedDay = target.day();

    if (controls.yearCombo)
    {
        const int yearIndex = ensureYearOption(controls.yearCombo, target.year());
        const QSignalBlocker blocker(controls.yearCombo);
        controls.yearCombo->setCurrentIndex(yearIndex >= 0 ? yearIndex : 0);
    }

    if (controls.monthCombo)
    {
        const int monthIndex = controls.monthCombo->findData(target.month());
        const QSignalBlocker blocker(controls.monthCombo);
        controls.monthCombo->setCurrentIndex(monthIndex >= 0 ? monthIndex : 0);
    }
}

QDate SessionEditorDialog::extractDate(const DateTimeControls &controls) const
{
    if (!controls.yearCombo || !controls.monthCombo)
        return {};

    const int year = controls.yearCombo->currentData().toInt();
    const int month = controls.monthCombo->currentData().toInt();
    if (year <= 0 || month <= 0)
        return {};

    const int daysInMonth = QDate(year, month, 1).daysInMonth();
    const int clampedDay = std::clamp(controls.storedDay > 0 ? controls.storedDay : 1, 1, daysInMonth);
    return QDate(year, month, clampedDay);
}
