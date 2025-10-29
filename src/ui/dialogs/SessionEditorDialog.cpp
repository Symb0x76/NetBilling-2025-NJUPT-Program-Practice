#include "ui/dialogs/SessionEditorDialog.h"

#include "ElaAppBar.h"
#include "ElaComboBox.h"
#include "ElaPushButton.h"
#include "ElaText.h"
#include "ui/ThemeUtils.h"

#include <QDateTimeEdit>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QVariant>
#include <QVBoxLayout>
#include <QDateTime>

SessionEditorDialog::SessionEditorDialog(const QHash<QString, QString> &accountNames, QWidget *parent)
    : ElaDialog(parent), m_accountNames(accountNames)
{
    setWindowTitle(QStringLiteral(u"编辑上网记录"));
    setFixedSize(460, 300);
    setWindowButtonFlag(ElaAppBarType::ThemeChangeButtonHint);
    connect(this, &SessionEditorDialog::themeChangeButtonClicked, this, [this]
            { toggleThemeMode(this); });
    setupUi();
}

void SessionEditorDialog::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);

    auto *formLayout = new QFormLayout();
    formLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    formLayout->setFormAlignment(Qt::AlignTop);
    formLayout->setVerticalSpacing(12);

    m_accountCombo = new ElaComboBox(this);
    m_accountCombo->setEditable(true);
    for (auto it = m_accountNames.constBegin(); it != m_accountNames.constEnd(); ++it)
    {
        const QString display = it.value().isEmpty()
                                    ? it.key()
                                    : QStringLiteral("%1 (%2)").arg(it.key(), it.value());
        m_accountCombo->addItem(display, it.key());
    }
    formLayout->addRow(createFormLabel(QStringLiteral(u"账号"), this), m_accountCombo);

    m_beginEdit = new QDateTimeEdit(QDateTime::currentDateTime(), this);
    m_beginEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    m_beginEdit->setCalendarPopup(true);
    formLayout->addRow(createFormLabel(QStringLiteral(u"开始时间"), this), m_beginEdit);

    m_endEdit = new QDateTimeEdit(QDateTime::currentDateTime(), this);
    m_endEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    m_endEdit->setCalendarPopup(true);
    formLayout->addRow(createFormLabel(QStringLiteral(u"结束时间"), this), m_endEdit);

    layout->addLayout(formLayout);
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

    m_beginEdit->setDateTime(session.begin);
    m_endEdit->setDateTime(session.end);
}

Session SessionEditorDialog::session() const
{
    Session s;
    const QString account = m_accountCombo->currentData().toString();
    s.account = account.isEmpty() ? m_accountCombo->currentText().trimmed() : account;
    s.begin = m_beginEdit->dateTime();
    s.end = m_endEdit->dateTime();
    return s;
}

QString SessionEditorDialog::validate() const
{
    const QString account = m_accountCombo->currentData().toString().isEmpty()
                                ? m_accountCombo->currentText().trimmed()
                                : m_accountCombo->currentData().toString();
    if (account.isEmpty())
        return QStringLiteral(u"账号不能为空。");
    if (m_beginEdit->dateTime() > m_endEdit->dateTime())
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
