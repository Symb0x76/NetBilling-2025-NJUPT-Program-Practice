#include "ui/dialogs/login_dialog.h"

#include "ElaAppBar.h"
#include "ElaLineEdit.h"
#include "ElaPushButton.h"
#include "ElaText.h"
#include "ui/theme_utils.h"
#include "backend/repository.h"
#include "backend/security.h"
#include "ui/dialogs/password_dialog.h"
#include "ui/dialogs/register_dialog.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QVBoxLayout>

#include <algorithm>

LoginDialog::LoginDialog(QString dataDir, QString outDir, QWidget *parent)
    : ElaDialog(parent), m_dataDir(std::move(dataDir)), m_outDir(std::move(outDir)), m_repository(std::make_unique<Repository>(m_dataDir, m_outDir))
{
    setWindowTitle(QStringLiteral(u"上网计费系统登录"));
    setFixedSize(420, 320);
    setWindowButtonFlag(ElaAppBarType::ThemeChangeButtonHint);
    connect(this, &LoginDialog::themeChangeButtonClicked, this, [this]
            { toggleThemeMode(this); });
    setupUi();
    loadUsers();
    ensureDefaultAdmin();
    if (m_createdDefaultAdmin)
    {
        QMessageBox::information(this, windowTitle(),
                                 QStringLiteral(u"系统已自动创建默认管理员账号：admin / admin123。\n请及时修改默认密码。"));
    }
}

void LoginDialog::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);

    auto *title = new ElaText(QStringLiteral(u"请登录"), this);
    title->setTextStyle(ElaTextType::Title);
    layout->addWidget(title);

    auto *formLayout = new QFormLayout();
    formLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    formLayout->setVerticalSpacing(12);

    m_accountEdit = new ElaLineEdit(this);
    m_accountEdit->setPlaceholderText(QStringLiteral(u"账号"));
    formLayout->addRow(createFormLabel(QStringLiteral(u"账号"), this), m_accountEdit);

    m_passwordEdit = new ElaLineEdit(this);
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setPlaceholderText(QStringLiteral(u"密码"));
    formLayout->addRow(createFormLabel(QStringLiteral(u"密码"), this), m_passwordEdit);

    layout->addLayout(formLayout);

    m_loginButton = new ElaPushButton(QStringLiteral(u"登录"), this);
    m_loginButton->setDefault(true);
    m_registerButton = new ElaPushButton(QStringLiteral(u"新用户注册"), this);
    m_changePasswordButton = new ElaPushButton(QStringLiteral(u"修改密码"), this);

    auto *buttonRow = new QHBoxLayout();
    buttonRow->addWidget(m_loginButton, 1);
    buttonRow->addSpacing(12);
    buttonRow->addWidget(m_registerButton, 1);
    buttonRow->addSpacing(12);
    buttonRow->addWidget(m_changePasswordButton, 1);
    layout->addLayout(buttonRow);

    connect(m_loginButton, &ElaPushButton::clicked, this, &LoginDialog::handleLogin);
    connect(m_registerButton, &ElaPushButton::clicked, this, &LoginDialog::handleRegister);
    connect(m_changePasswordButton, &ElaPushButton::clicked, this, &LoginDialog::handleChangePassword);
}

void LoginDialog::loadUsers()
{
    m_users = m_repository->loadUsers();
}

void LoginDialog::ensureDefaultAdmin()
{
    auto it = std::find_if(m_users.begin(), m_users.end(), [](const User &u)
                           { return u.role == UserRole::Admin; });
    if (it != m_users.end())
        return;

    User admin;
    admin.name = QStringLiteral(u"系统管理员");
    admin.account = QStringLiteral("admin");
    admin.plan = Tariff::NoDiscount;
    admin.passwordHash = Security::hashPassword(QStringLiteral("admin123"));
    admin.role = UserRole::Admin;
    admin.enabled = true;
    admin.balance = 0.0;
    m_users.push_back(admin);
    saveUsers();
    m_createdDefaultAdmin = true;
}

bool LoginDialog::saveUsers()
{
    return m_repository->saveUsers(m_users);
}

User *LoginDialog::findUser(const QString &account)
{
    auto it = std::find_if(m_users.begin(), m_users.end(), [&](const User &u)
                           { return u.account.compare(account, Qt::CaseInsensitive) == 0; });
    if (it == m_users.end())
        return nullptr;
    return &(*it);
}

const User *LoginDialog::findUser(const QString &account) const
{
    auto it = std::find_if(m_users.cbegin(), m_users.cend(), [&](const User &u)
                           { return u.account.compare(account, Qt::CaseInsensitive) == 0; });
    if (it == m_users.cend())
        return nullptr;
    return &(*it);
}

bool LoginDialog::isAuthenticated() const
{
    return m_authenticated;
}

User LoginDialog::authenticatedUser() const
{
    return m_loggedInUser;
}

QString LoginDialog::dataDir() const
{
    return m_dataDir;
}

QString LoginDialog::outputDir() const
{
    return m_outDir;
}

void LoginDialog::handleLogin()
{
    const QString account = m_accountEdit->text().trimmed();
    const QString password = m_passwordEdit->text();

    if (account.isEmpty() || password.isEmpty())
    {
        QMessageBox::warning(this, windowTitle(), QStringLiteral(u"请输入账号和密码。"));
        return;
    }

    const User *user = findUser(account);
    if (!user)
    {
        QMessageBox::warning(this, windowTitle(), QStringLiteral(u"账号不存在，请先注册。"));
        return;
    }
    if (!user->enabled)
    {
        QMessageBox::warning(this, windowTitle(), QStringLiteral(u"该账号已被停用，请联系管理员。"));
        return;
    }
    if (!Security::verifyPassword(password, user->passwordHash))
    {
        QMessageBox::warning(this, windowTitle(), QStringLiteral(u"密码错误。"));
        return;
    }

    m_loggedInUser = *user;
    m_authenticated = true;
    accept();
}

void LoginDialog::handleRegister()
{
    RegistrationDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    User newUser = dialog.user();
    if (findUser(newUser.account))
    {
        QMessageBox::warning(this, windowTitle(), QStringLiteral(u"该账号已存在，请更换其他账号。"));
        return;
    }
    m_users.push_back(std::move(newUser));
    if (!saveUsers())
    {
        QMessageBox::warning(this, windowTitle(), QStringLiteral(u"保存注册信息失败，请检查数据目录权限。"));
        m_users.pop_back();
        return;
    }
    QMessageBox::information(this, windowTitle(), QStringLiteral(u"注册成功，请使用新账号登录。"));
}

void LoginDialog::handleChangePassword()
{
    ChangePasswordDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    User *user = findUser(dialog.account());
    if (!user)
    {
        QMessageBox::warning(this, windowTitle(), QStringLiteral(u"账号不存在。"));
        return;
    }
    if (!Security::verifyPassword(dialog.oldPassword(), user->passwordHash))
    {
        QMessageBox::warning(this, windowTitle(), QStringLiteral(u"当前密码不正确。"));
        return;
    }

    user->passwordHash = Security::hashPassword(dialog.newPassword());
    if (!saveUsers())
    {
        QMessageBox::warning(this, windowTitle(), QStringLiteral(u"保存密码失败，请稍后重试。"));
        return;
    }
    QMessageBox::information(this, windowTitle(), QStringLiteral(u"密码已更新。"));
}
