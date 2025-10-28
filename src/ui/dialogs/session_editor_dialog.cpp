#include "ui/dialogs/session_editor_dialog.h"

#include "ElaComboBox.h"
#include "ElaPushButton.h"

#include <QDateTimeEdit>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QVariant>
#include <QVBoxLayout>
#include <QDateTime>

SessionEditorDialog::SessionEditorDialog(const QHash<QString, QString>& accountNames, QWidget* parent)
    : ElaDialog(parent)
    , m_accountNames(accountNames)
{
    setWindowTitle(QStringLiteral(u8"编辑上网记录"));
    setFixedSize(460, 300);
    setupUi();
}

void SessionEditorDialog::setupUi()
{
    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);

    auto* formLayout = new QFormLayout();
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
    formLayout->addRow(QStringLiteral(u8"账号"), m_accountCombo);

    m_beginEdit = new QDateTimeEdit(QDateTime::currentDateTime(), this);
    m_beginEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    m_beginEdit->setCalendarPopup(true);
    formLayout->addRow(QStringLiteral(u8"开始时间"), m_beginEdit);

    m_endEdit = new QDateTimeEdit(QDateTime::currentDateTime(), this);
    m_endEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    m_endEdit->setCalendarPopup(true);
    formLayout->addRow(QStringLiteral(u8"结束时间"), m_endEdit);

    layout->addLayout(formLayout);
    layout->addStretch();

    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    auto* cancelButton = new ElaPushButton(QStringLiteral(u8"取消"), this);
    auto* okButton = new ElaPushButton(QStringLiteral(u8"确定"), this);
    buttonLayout->addWidget(cancelButton);
    buttonLayout->addWidget(okButton);
    layout->addLayout(buttonLayout);

    setCentralWidget(central);

    connect(cancelButton, &ElaPushButton::clicked, this, &SessionEditorDialog::reject);
    connect(okButton, &ElaPushButton::clicked, this, &SessionEditorDialog::accept);
}

void SessionEditorDialog::setSession(const Session& session)
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
        return QStringLiteral(u8"账号不能为空。");
    if (m_beginEdit->dateTime() > m_endEdit->dateTime())
        return QStringLiteral(u8"结束时间必须晚于开始时间。");
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
