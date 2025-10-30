#include "ui/dialogs/RegisterDialog.h"

#include "ElaAppBar.h"
#include "ElaComboBox.h"
#include "ElaLineEdit.h"
#include "ElaPushButton.h"
#include "ElaText.h"
#include "backend/Security.h"
#include "ui/ThemeUtils.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QRegularExpressionValidator>
#include <QVariant>
#include <QVBoxLayout>

RegistrationDialog::RegistrationDialog(QWidget *parent)
    : ElaDialog(parent)
{
    setWindowTitle(QStringLiteral(u"注册账号"));
    setFixedSize(420, 360);
    setWindowButtonFlag(ElaAppBarType::ThemeChangeButtonHint);
    connect(this, &RegistrationDialog::themeChangeButtonClicked, this, [this]
            { toggleThemeMode(this); });
    setupUi();
}

void RegistrationDialog::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);

    auto *formLayout = new QFormLayout();
    formLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    formLayout->setVerticalSpacing(12);

    m_nameEdit = new ElaLineEdit(this);
    m_nameEdit->setPlaceholderText(QStringLiteral(u"请输入姓名"));
    formLayout->addRow(createFormLabel(QStringLiteral(u"姓名"), this), m_nameEdit);

    m_accountEdit = new ElaLineEdit(this);
    m_accountEdit->setPlaceholderText(QStringLiteral(u"账号，仅支持字母和数字"));
    auto *accountValidator = new QRegularExpressionValidator(m_accountPattern, m_accountEdit);
    m_accountEdit->setValidator(accountValidator);
    formLayout->addRow(createFormLabel(QStringLiteral(u"账号"), this), m_accountEdit);

    m_planCombo = new ElaComboBox(this);
    m_planCombo->addItem(QStringLiteral(u"标准计费"), QVariant::fromValue(static_cast<int>(Tariff::NoDiscount)));
    m_planCombo->addItem(QStringLiteral(u"30 小时套餐"), QVariant::fromValue(static_cast<int>(Tariff::Pack30h)));
    m_planCombo->addItem(QStringLiteral(u"60 小时套餐"), QVariant::fromValue(static_cast<int>(Tariff::Pack60h)));
    m_planCombo->addItem(QStringLiteral(u"150 小时套餐"), QVariant::fromValue(static_cast<int>(Tariff::Pack150h)));
    m_planCombo->addItem(QStringLiteral(u"包月不限时"), QVariant::fromValue(static_cast<int>(Tariff::Unlimited)));
    formLayout->addRow(createFormLabel(QStringLiteral(u"计费套餐"), this), m_planCombo);

    m_passwordEdit = new ElaLineEdit(this);
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setPlaceholderText(QStringLiteral(u"设置登录密码"));
    attachPasswordVisibilityToggle(m_passwordEdit);
    formLayout->addRow(createFormLabel(QStringLiteral(u"密码"), this), m_passwordEdit);

    m_confirmEdit = new ElaLineEdit(this);
    m_confirmEdit->setEchoMode(QLineEdit::Password);
    m_confirmEdit->setPlaceholderText(QStringLiteral(u"再次输入密码"));
    attachPasswordVisibilityToggle(m_confirmEdit);
    formLayout->addRow(createFormLabel(QStringLiteral(u"确认密码"), this), m_confirmEdit);

    layout->addLayout(formLayout);
    layout->addStretch();

    auto *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    auto *cancelButton = new ElaPushButton(QStringLiteral(u"取消"), this);
    auto *okButton = new ElaPushButton(QStringLiteral(u"完成注册"), this);
    buttonLayout->addWidget(cancelButton);
    buttonLayout->addWidget(okButton);
    layout->addLayout(buttonLayout);

    connect(cancelButton, &ElaPushButton::clicked, this, &RegistrationDialog::reject);
    connect(okButton, &ElaPushButton::clicked, this, &RegistrationDialog::accept);
}

QString RegistrationDialog::validate() const
{
    const QString name = m_nameEdit->text().trimmed();
    if (name.isEmpty())
        return QStringLiteral(u"姓名不能为空。");
    if (name.contains(QLatin1Char(',')))
        return QStringLiteral(u"姓名不能包含逗号。");
    const QString account = m_accountEdit->text().trimmed();
    if (account.isEmpty())
        return QStringLiteral(u"账号不能为空。");
    if (account.contains(QLatin1Char(',')))
        return QStringLiteral(u"账号不能包含逗号。");
    const QRegularExpression strictPattern(QStringLiteral("^[A-Za-z0-9]+$"));
    if (!strictPattern.match(account).hasMatch())
        return QStringLiteral(u"账号只能包含数字和英文字母。");
    if (m_existingAccountsLower.contains(account.toLower()))
        return QStringLiteral(u"账号已存在，请更换其他账号。");
    if (m_passwordEdit->text().length() < 6)
        return QStringLiteral(u"密码长度至少 6 位。");
    if (m_passwordEdit->text() != m_confirmEdit->text())
        return QStringLiteral(u"两次输入的密码不一致。");
    return {};
}

void RegistrationDialog::accept()
{
    const auto error = validate();
    if (!error.isEmpty())
    {
        QMessageBox::warning(this, windowTitle(), error);
        return;
    }
    ElaDialog::accept();
}

void RegistrationDialog::setExistingAccounts(const QStringList &accounts)
{
    m_existingAccountsLower.clear();
    for (const QString &rawAccount : accounts)
    {
        const QString trimmed = rawAccount.trimmed();
        if (trimmed.isEmpty())
            continue;
        m_existingAccountsLower.insert(trimmed.toLower());
    }
}

User RegistrationDialog::user() const
{
    User u;
    u.name = m_nameEdit->text().trimmed();
    u.account = m_accountEdit->text().trimmed();
    u.plan = static_cast<Tariff>(m_planCombo->currentData().toInt());
    u.passwordHash = Security::hashPassword(m_passwordEdit->text());
    u.role = UserRole::User;
    u.enabled = true;
    u.balance = 0.0;
    return u;
}
