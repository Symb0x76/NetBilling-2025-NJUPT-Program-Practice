#include "ui/dialogs/SessionEditorDialog.h"

#include "ElaAppBar.h"
#include "ElaCalendarPicker.h"
#include "ElaPushButton.h"
#include "ElaText.h"
#include "ElaTheme.h"
#include "ElaIcon.h"
#include "ElaComboBox.h"
#include "ElaLineEdit.h"
#include "ElaIconButton.h"
#include "ui/ThemeUtils.h"

#include <QDate>
#include <QDateTime>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QTime>
#include <QPalette>
#include <QWidget>
#include <QVBoxLayout>
#include <QSizePolicy>
#include <QKeyEvent>
#include <QLineEdit>
#include <QEvent>

#include <algorithm>

namespace
{
    constexpr int kDaySeconds = 24 * 60 * 60;

    int toSeconds(const QTime &time)
    {
        if (!time.isValid())
            return 0;
        return QTime(0, 0).secsTo(time);
    }

    QTime fromSeconds(int seconds)
    {
        seconds = std::clamp(seconds, 0, kDaySeconds - 1);
#if QT_VERSION >= QT_VERSION_CHECK(5, 8, 0)
        return QTime::fromMSecsSinceStartOfDay(seconds * 1000);
#else
        return QTime(0, 0).addSecs(seconds);
#endif
    }
} // namespace

class SessionEditorDialog::TimeSelectorWidget : public QWidget
{
public:
    explicit TimeSelectorWidget(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

        auto *layout = new QHBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(8);

        m_edit = new ElaLineEdit(this);
        m_edit->setAlignment(Qt::AlignCenter);
        m_edit->setBorderRadius(6);
        m_edit->setIsClearButtonEnable(false);
        m_edit->setFixedHeight(36);
        m_edit->installEventFilter(this);
        layout->addWidget(m_edit, 1);

        auto *buttonBox = new QWidget(this);
        buttonBox->setFixedWidth(32);
        buttonBox->setFixedHeight(36);
        auto *buttonsLayout = new QVBoxLayout(buttonBox);
        buttonsLayout->setContentsMargins(0, 0, 0, 0);
        buttonsLayout->setSpacing(4);

        m_upButton = new ElaIconButton(ElaIconType::ChevronUp, 16, 32, 16, buttonBox);
        m_downButton = new ElaIconButton(ElaIconType::ChevronDown, 16, 32, 16, buttonBox);
        m_upButton->setBorderRadius(6);
        m_downButton->setBorderRadius(6);
        buttonsLayout->addWidget(m_upButton);
        buttonsLayout->addWidget(m_downButton);
        layout->addWidget(buttonBox);

        setFocusProxy(m_edit);

        connect(m_upButton, &QPushButton::clicked, this, [this]()
                { adjust(+1); });
        connect(m_downButton, &QPushButton::clicked, this, [this]()
                { adjust(-1); });
        connect(m_edit, &QLineEdit::editingFinished, this, [this]()
                { commitEditor(); });
        connect(eTheme, &ElaTheme::themeModeChanged, this, [this](ElaThemeType::ThemeMode)
                { updateTheme(); });

        updateTheme();
        setRange(QTime(0, 0, 0), QTime(23, 59, 59));
        setTime(QTime(0, 0, 0));
    }

    void setTime(const QTime &time)
    {
        const SelectionState state = captureSelection();
        const int clamped = clampSeconds(toSeconds(time));
        const QTime normalized = fromSeconds(clamped);
        if (!normalized.isValid())
            return;
        m_time = normalized;
        updateDisplay();
        restoreSelection(state);
    }

    QTime time() const
    {
        return m_time;
    }

    void setRange(const QTime &min, const QTime &max)
    {
        m_minSeconds = std::clamp(toSeconds(min.isValid() ? min : QTime(0, 0, 0)), 0, kDaySeconds - 1);
        m_maxSeconds = std::clamp(toSeconds(max.isValid() ? max : QTime(23, 59, 59)), m_minSeconds, kDaySeconds - 1);
        if (m_time.isValid())
        {
            const int clamped = clampSeconds(toSeconds(m_time));
            m_time = fromSeconds(clamped);
            updateDisplay();
        }
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (watched == m_edit && event->type() == QEvent::KeyPress)
        {
            const auto key = static_cast<QKeyEvent *>(event);
            if (key->key() == Qt::Key_Up)
            {
                adjust(+1);
                return true;
            }
            if (key->key() == Qt::Key_Down)
            {
                adjust(-1);
                return true;
            }
        }
        return QWidget::eventFilter(watched, event);
    }

private:
    struct SelectionState
    {
        int cursor{-1};
        int start{-1};
        int length{0};
    };

    void adjust(int direction)
    {
        const SelectionState state = captureSelection();
        const int stepSeconds = stepSizeForCursor();
        if (stepSeconds == 0)
            return;
        const int current = clampSeconds(toSeconds(m_time));
        const int target = clampSeconds(current + direction * stepSeconds);
        if (target == current)
            return;
        m_time = fromSeconds(target);
        updateDisplay();
        restoreSelection(state);
    }

    int stepSizeForCursor() const
    {
        int pos = m_edit->cursorPosition();
        if (m_edit->hasSelectedText())
            pos = m_edit->selectionStart();
        if (pos <= 2)
            return 3600;
        if (pos <= 5)
            return 60;
        return 1;
    }

    SelectionState captureSelection() const
    {
        SelectionState state;
        state.cursor = m_edit->cursorPosition();
        if (m_edit->hasSelectedText())
        {
            state.start = m_edit->selectionStart();
            state.length = m_edit->selectedText().length();
        }
        return state;
    }

    void restoreSelection(const SelectionState &state)
    {
        if (state.start >= 0 && state.length > 0)
        {
            m_edit->setSelection(state.start, state.length);
        }
        else if (state.cursor >= 0)
        {
            m_edit->setCursorPosition(state.cursor);
        }
    }

    void commitEditor()
    {
        const SelectionState state = captureSelection();
        const QTime candidate = QTime::fromString(m_edit->text().trimmed(), m_format);
        if (candidate.isValid())
        {
            m_time = fromSeconds(clampSeconds(toSeconds(candidate)));
        }
        updateDisplay();
        restoreSelection(state);
    }

    void updateDisplay()
    {
        m_edit->blockSignals(true);
        m_edit->setText(m_time.isValid() ? m_time.toString(m_format) : QString());
        m_edit->blockSignals(false);
    }

    void updateTheme()
    {
        const auto mode = eTheme->getThemeMode();
        const QColor base = eTheme->getThemeColor(mode, ElaThemeType::BasicBase);
        const QColor text = eTheme->getThemeColor(mode, ElaThemeType::BasicText);
        const QColor border = eTheme->getThemeColor(mode, ElaThemeType::BasicBorder);
        const QColor hover = eTheme->getThemeColor(mode, ElaThemeType::PrimaryHover);
        const QColor focus = eTheme->getThemeColor(mode, ElaThemeType::PrimaryNormal);
        const QColor disabled = eTheme->getThemeColor(mode, ElaThemeType::BasicTextDisable);

        QPalette palette = m_edit->palette();
        palette.setColor(QPalette::Base, base);
        palette.setColor(QPalette::Text, text);
        palette.setColor(QPalette::Highlight, focus);
        palette.setColor(QPalette::HighlightedText, text);
        palette.setColor(QPalette::Disabled, QPalette::Text, disabled);
        palette.setColor(QPalette::Disabled, QPalette::Base, base);
        m_edit->setPalette(palette);

        m_edit->setStyleSheet(QStringLiteral(
                                  "ElaLineEdit { background: %1; color: %2; border: 1px solid %3; border-radius: 6px; padding: 4px 8px; }"
                                  "ElaLineEdit:hover { border-color: %4; }"
                                  "ElaLineEdit:focus { border-color: %5; }"
                                  "ElaLineEdit:disabled { color: %6; }")
                                  .arg(base.name(),
                                       text.name(),
                                       border.name(),
                                       hover.name(),
                                       focus.name(),
                                       disabled.name()));

        auto styleButton = [&](ElaIconButton *button)
        {
            if (!button)
                return;
            button->setLightHoverColor(hover);
            button->setDarkHoverColor(hover);
            button->setLightIconColor(text);
            button->setDarkIconColor(text);
            button->setLightHoverIconColor(text);
            button->setDarkHoverIconColor(text);
        };
        styleButton(m_upButton);
        styleButton(m_downButton);
    }

    int clampSeconds(int value) const
    {
        return std::max(m_minSeconds, std::min(m_maxSeconds, value));
    }

    ElaLineEdit *m_edit{nullptr};
    ElaIconButton *m_upButton{nullptr};
    ElaIconButton *m_downButton{nullptr};
    QTime m_time;
    QString m_format{QStringLiteral("HH:mm:ss")};
    int m_minSeconds{0};
    int m_maxSeconds{kDaySeconds - 1};
};

SessionEditorDialog::SessionEditorDialog(const QHash<QString, QString> &accountNames, QWidget *parent)
    : ElaDialog(parent), m_accountNames(accountNames)
{
    setWindowTitle(QStringLiteral(u"编辑上网记录"));
    setMinimumSize(480, 400);
    resize(480, 400);
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
    m_accountCombo->setFixedHeight(36);
    m_accountCombo->setSizeAdjustPolicy(QComboBox::AdjustToContentsOnFirstShow);
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

    m_beginControls.timeSelector = createTimeSelector(now);
    m_endControls.timeSelector = createTimeSelector(now.addSecs(3600));

    auto *beginDateSelector = createDateSelector(m_beginControls);
    auto *endDateSelector = createDateSelector(m_endControls);

    auto *pickerLayout = new QFormLayout();
    pickerLayout->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    pickerLayout->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
    pickerLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    pickerLayout->setHorizontalSpacing(16);
    pickerLayout->setVerticalSpacing(12);
    pickerLayout->setContentsMargins(0, 0, 0, 0);

    auto *beginDateLabel = createFormLabel(QStringLiteral(u"开始日期"), this);
    beginDateLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    auto *beginTimeLabel = createFormLabel(QStringLiteral(u"开始时间"), this);
    beginTimeLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    auto *endDateLabel = createFormLabel(QStringLiteral(u"结束日期"), this);
    endDateLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    auto *endTimeLabel = createFormLabel(QStringLiteral(u"结束时间"), this);
    endTimeLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    pickerLayout->addRow(beginDateLabel, beginDateSelector);
    pickerLayout->addRow(beginTimeLabel, m_beginControls.timeSelector);
    pickerLayout->addRow(endDateLabel, endDateSelector);
    pickerLayout->addRow(endTimeLabel, m_endControls.timeSelector);

    layout->addLayout(pickerLayout);

    layout->addStretch();

    auto *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    auto *okButton = new ElaPushButton(QStringLiteral(u"确定"), this);
    okButton->setDefault(true);
    auto *cancelButton = new ElaPushButton(QStringLiteral(u"取消"), this);
    buttonLayout->addWidget(okButton);
    buttonLayout->addWidget(cancelButton);
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
        if (m_beginControls.timeSelector)
            m_beginControls.timeSelector->setTime(session.begin.time());
        setDate(m_beginControls, session.begin.date());
    }

    if (session.end.isValid())
    {
        if (m_endControls.timeSelector)
            m_endControls.timeSelector->setTime(session.end.time());
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
    const QDateTime begin(beginDate, m_beginControls.timeSelector ? m_beginControls.timeSelector->time() : QTime());
    const QDateTime end(endDate, m_endControls.timeSelector ? m_endControls.timeSelector->time() : QTime());
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
        return QStringLiteral(u"请选择有效的开始和结束日期。");
    const QDateTime begin(beginDate, m_beginControls.timeSelector ? m_beginControls.timeSelector->time() : QTime());
    const QDateTime end(endDate, m_endControls.timeSelector ? m_endControls.timeSelector->time() : QTime());
    if (!begin.isValid() || !end.isValid())
        return QStringLiteral(u"请选择有效的开始和结束时间。");
    if (begin > end)
        return QStringLiteral(u"结束时间必须晚于开始时间。");
    return {};
}

void SessionEditorDialog::accept()
{
    const auto error = validate();
    if (!error.isEmpty())
    {
        showThemedWarning(this, windowTitle(), error);
        return;
    }
    ElaDialog::accept();
}

QWidget *SessionEditorDialog::createDateSelector(DateTimeControls &controls)
{
    auto *picker = new ElaCalendarPicker(this);
    styleCalendarPicker(picker);
    controls.datePicker = picker;
    return picker;
}

SessionEditorDialog::TimeSelectorWidget *SessionEditorDialog::createTimeSelector(const QTime &initialTime)
{
    auto *selector = new TimeSelectorWidget(this);
    selector->setRange(QTime(0, 0, 0), QTime(23, 59, 59));
    selector->setTime(initialTime);
    return selector;
}

void SessionEditorDialog::setDate(DateTimeControls &controls, const QDate &date)
{
    const QDate target = date.isValid() ? date : QDate::currentDate();
    if (controls.datePicker)
        controls.datePicker->setSelectedDate(target);
}

QDate SessionEditorDialog::extractDate(const DateTimeControls &controls) const
{
    if (!controls.datePicker)
        return {};

    return controls.datePicker->getSelectedDate();
}

void SessionEditorDialog::styleCalendarPicker(ElaCalendarPicker *picker) const
{
    if (!picker)
        return;

    picker->setBorderRadius(6);
    picker->setMinimumWidth(0);
    picker->setMaximumWidth(QWIDGETSIZE_MAX);
    picker->setMinimumHeight(36);
    picker->setMaximumHeight(36);
    picker->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    picker->setCursor(Qt::PointingHandCursor);
}
