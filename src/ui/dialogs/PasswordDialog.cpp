#include "ui/dialogs/PasswordDialog.h"

#include "ElaAppBar.h"
#include "ElaLineEdit.h"
#include "ElaPushButton.h"
#include "ElaText.h"
#include "ui/ThemeUtils.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QVBoxLayout>

ChangePasswordDialog::ChangePasswordDialog(QWidget *parent)
    : ElaDialog(parent)
{
    setWindowTitle(QStringLiteral(u"修改密码"));
    setFixedSize(420, 320);
    setWindowButtonFlag(ElaAppBarType::ThemeChangeButtonHint);
    connect(this, &ChangePasswordDialog::themeChangeButtonClicked, this, [this]
            { toggleThemeMode(this); });
    setupUi();
}

void ChangePasswordDialog::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);

    auto *formLayout = new QFormLayout();
    formLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    formLayout->setVerticalSpacing(12);

    m_accountEdit = new ElaLineEdit(this);
    m_accountEdit->setPlaceholderText(QStringLiteral(u"请输入账号"));
    if (!m_prefillAccount.isEmpty())
        m_accountEdit->setText(m_prefillAccount);
    formLayout->addRow(createFormLabel(QStringLiteral(u"账号"), this), m_accountEdit);

    m_oldPasswordEdit = new ElaLineEdit(this);
    m_oldPasswordEdit->setEchoMode(QLineEdit::Password);
    formLayout->addRow(createFormLabel(QStringLiteral(u"当前密码"), this), m_oldPasswordEdit);

    m_newPasswordEdit = new ElaLineEdit(this);
    m_newPasswordEdit->setEchoMode(QLineEdit::Password);
    formLayout->addRow(createFormLabel(QStringLiteral(u"新密码"), this), m_newPasswordEdit);

    m_confirmPasswordEdit = new ElaLineEdit(this);
    m_confirmPasswordEdit->setEchoMode(QLineEdit::Password);
    formLayout->addRow(createFormLabel(QStringLiteral(u"确认密码"), this), m_confirmPasswordEdit);

    layout->addLayout(formLayout);
    layout->addStretch();

    auto *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    auto *cancelButton = new ElaPushButton(QStringLiteral(u"取消"), this);
    auto *okButton = new ElaPushButton(QStringLiteral(u"确认修改"), this);
    buttonLayout->addWidget(cancelButton);
    buttonLayout->addWidget(okButton);
    layout->addLayout(buttonLayout);

    connect(cancelButton, &ElaPushButton::clicked, this, &ChangePasswordDialog::reject);
    connect(okButton, &ElaPushButton::clicked, this, &ChangePasswordDialog::accept);
}

QString ChangePasswordDialog::validate() const
{
    if (m_accountEdit->text().trimmed().isEmpty())
        return QStringLiteral(u"账号不能为空。");
    if (m_oldPasswordEdit->text().isEmpty())
        return QStringLiteral(u"请输入当前密码。");
    if (m_newPasswordEdit->text().length() < 6)
        return QStringLiteral(u"新密码长度至少 6 位。");
    if (m_newPasswordEdit->text() != m_confirmPasswordEdit->text())
        return QStringLiteral(u"两次输入的新密码不一致。");
    if (m_newPasswordEdit->text() == m_oldPasswordEdit->text())
        return QStringLiteral(u"新密码不能与旧密码相同。");
    return {};
}

void ChangePasswordDialog::accept()
{
    const auto error = validate();
    if (!error.isEmpty())
    {
        QMessageBox::warning(this, windowTitle(), error);
        return;
    }
    ElaDialog::accept();
}

QString ChangePasswordDialog::account() const
{
    return m_accountEdit->text().trimmed();
}

QString ChangePasswordDialog::oldPassword() const
{
    return m_oldPasswordEdit->text();
}

QString ChangePasswordDialog::newPassword() const
{
    return m_newPasswordEdit->text();
}

void ChangePasswordDialog::setAccount(const QString &account)
{
    m_prefillAccount = account;
    if (m_accountEdit)
        m_accountEdit->setText(account);
}
