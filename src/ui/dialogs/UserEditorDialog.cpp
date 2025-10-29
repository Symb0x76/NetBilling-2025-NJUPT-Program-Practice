#include "ui/dialogs/UserEditorDialog.h"

#include "ElaAppBar.h"
#include "ElaCheckBox.h"
#include "ElaComboBox.h"
#include "ElaLineEdit.h"
#include "ElaPushButton.h"
#include "ElaText.h"
#include "backend/Security.h"
#include "ui/ThemeUtils.h"

#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QVariant>
#include <QVBoxLayout>

namespace
{
    QString roleText(UserRole role)
    {
        switch (role)
        {
        case UserRole::Admin:
            return QStringLiteral(u"管理员");
        case UserRole::User:
        default:
            return QStringLiteral(u"普通用户");
        }
    }
} // namespace

UserEditorDialog::UserEditorDialog(Mode mode, QWidget *parent)
    : ElaDialog(parent), m_mode(mode)
{
    setWindowTitle(mode == Mode::Create ? QStringLiteral(u"新增用户") : QStringLiteral(u"编辑用户"));
    setFixedSize(460, 420);
    setWindowButtonFlag(ElaAppBarType::ThemeChangeButtonHint);
    connect(this, &UserEditorDialog::themeChangeButtonClicked, this, [this]
            { toggleThemeMode(this); });
    setupUi();
}

void UserEditorDialog::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);

    auto *formLayout = new QFormLayout();
    formLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    formLayout->setFormAlignment(Qt::AlignTop);
    formLayout->setVerticalSpacing(12);

    m_nameEdit = new ElaLineEdit(this);
    m_nameEdit->setPlaceholderText(QStringLiteral(u"请输入用户姓名"));
    formLayout->addRow(createFormLabel(QStringLiteral(u"姓名"), this), m_nameEdit);

    m_accountEdit = new ElaLineEdit(this);
    m_accountEdit->setPlaceholderText(QStringLiteral(u"登录账号，需唯一"));
    formLayout->addRow(createFormLabel(QStringLiteral(u"账号"), this), m_accountEdit);

    m_planCombo = new ElaComboBox(this);
    m_planCombo->addItem(QStringLiteral(u"标准计费"), QVariant::fromValue(static_cast<int>(Tariff::NoDiscount)));
    m_planCombo->addItem(QStringLiteral(u"30 小时套餐"), QVariant::fromValue(static_cast<int>(Tariff::Pack30h)));
    m_planCombo->addItem(QStringLiteral(u"60 小时套餐"), QVariant::fromValue(static_cast<int>(Tariff::Pack60h)));
    m_planCombo->addItem(QStringLiteral(u"150 小时套餐"), QVariant::fromValue(static_cast<int>(Tariff::Pack150h)));
    m_planCombo->addItem(QStringLiteral(u"包月不限时"), QVariant::fromValue(static_cast<int>(Tariff::Unlimited)));
    formLayout->addRow(createFormLabel(QStringLiteral(u"计费套餐"), this), m_planCombo);

    m_roleCombo = new ElaComboBox(this);
    m_roleCombo->addItem(roleText(UserRole::Admin), QVariant::fromValue(static_cast<int>(UserRole::Admin)));
    m_roleCombo->addItem(roleText(UserRole::User), QVariant::fromValue(static_cast<int>(UserRole::User)));
    formLayout->addRow(createFormLabel(QStringLiteral(u"角色"), this), m_roleCombo);

    m_enabledCheck = new ElaCheckBox(QStringLiteral(u"启用此账号"), this);
    m_enabledCheck->setChecked(true);
    formLayout->addRow(createFormLabel(QString(), this), m_enabledCheck);

    m_balanceSpin = new QDoubleSpinBox(this);
    m_balanceSpin->setRange(-1000000.0, 1000000.0);
    m_balanceSpin->setDecimals(2);
    m_balanceSpin->setSuffix(QStringLiteral(u" 元"));
    m_balanceSpin->setValue(0.0);
    formLayout->addRow(createFormLabel(QStringLiteral(u"账户余额"), this), m_balanceSpin);

    m_passwordEdit = new ElaLineEdit(this);
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setPlaceholderText(m_mode == Mode::Create ? QStringLiteral(u"设置登录密码") : QStringLiteral(u"留空则不修改"));
    formLayout->addRow(createFormLabel(QStringLiteral(u"登录密码"), this), m_passwordEdit);

    m_confirmPasswordEdit = new ElaLineEdit(this);
    m_confirmPasswordEdit->setEchoMode(QLineEdit::Password);
    m_confirmPasswordEdit->setPlaceholderText(QStringLiteral(u"再次输入密码"));
    formLayout->addRow(createFormLabel(QStringLiteral(u"确认密码"), this), m_confirmPasswordEdit);

    layout->addLayout(formLayout);
    layout->addStretch();

    auto *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    auto *cancelButton = new ElaPushButton(QStringLiteral(u"取消"), this);
    auto *okButton = new ElaPushButton(QStringLiteral(u"确定"), this);
    buttonLayout->addWidget(cancelButton);
    buttonLayout->addWidget(okButton);
    layout->addLayout(buttonLayout);

    if (m_mode == Mode::Edit)
    {
        m_accountEdit->setReadOnly(true);
    }

    connect(cancelButton, &ElaPushButton::clicked, this, &UserEditorDialog::reject);
    connect(okButton, &ElaPushButton::clicked, this, &UserEditorDialog::accept);
}

void UserEditorDialog::setUser(const User &user)
{
    m_nameEdit->setText(user.name);
    m_accountEdit->setText(user.account);
    const int planIndex = m_planCombo->findData(static_cast<int>(user.plan));
    if (planIndex >= 0)
        m_planCombo->setCurrentIndex(planIndex);

    const int roleIndex = m_roleCombo->findData(static_cast<int>(user.role));
    if (roleIndex >= 0)
        m_roleCombo->setCurrentIndex(roleIndex);
    else
        m_roleCombo->setCurrentIndex(1); // 普通用户

    m_enabledCheck->setChecked(user.enabled);
    m_balanceSpin->setValue(user.balance);
    m_originalPasswordHash = user.passwordHash;
}

User UserEditorDialog::user() const
{
    User result;
    result.name = m_nameEdit->text().trimmed();
    result.account = m_accountEdit->text().trimmed();
    result.plan = static_cast<Tariff>(m_planCombo->currentData().toInt());
    result.role = static_cast<UserRole>(m_roleCombo->currentData().toInt());
    result.enabled = m_enabledCheck->isChecked();
    result.balance = m_balanceSpin->value();

    const QString newPassword = m_passwordEdit->text();
    if (!newPassword.isEmpty())
    {
        result.passwordHash = Security::hashPassword(newPassword);
    }
    else
    {
        result.passwordHash = m_originalPasswordHash;
    }

    if (result.passwordHash.isEmpty())
        result.passwordHash = Security::hashPassword(QStringLiteral("123456"));

    return result;
}

QString UserEditorDialog::validate() const
{
    if (m_nameEdit->text().trimmed().isEmpty())
        return QStringLiteral(u"姓名不能为空。");
    if (m_accountEdit->text().trimmed().isEmpty())
        return QStringLiteral(u"账号不能为空。");

    const QString pwd = m_passwordEdit->text();
    const QString confirm = m_confirmPasswordEdit->text();
    if (m_mode == Mode::Create || !pwd.isEmpty() || !confirm.isEmpty())
    {
        if (pwd.length() < 6)
            return QStringLiteral(u"密码长度至少 6 位。");
        if (pwd != confirm)
            return QStringLiteral(u"两次输入的密码不一致。");
    }

    return {};
}

void UserEditorDialog::accept()
{
    const auto error = validate();
    if (!error.isEmpty())
    {
        QMessageBox::warning(this, windowTitle(), error);
        return;
    }
    ElaDialog::accept();
}
