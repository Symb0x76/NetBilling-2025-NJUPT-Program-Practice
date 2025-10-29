#pragma once

#include "ElaDialog.h"

#include "backend/models.h"
#include "backend/repository.h"

#include <memory>
#include <vector>

class ElaLineEdit;
class ElaPushButton;

class LoginDialog : public ElaDialog
{
    Q_OBJECT

public:
    LoginDialog(QString dataDir, QString outDir, QWidget *parent = nullptr);

    bool isAuthenticated() const;
    User authenticatedUser() const;
    QString dataDir() const;
    QString outputDir() const;

private slots:
    void handleLogin();
    void handleRegister();
    void handleChangePassword();

private:
    void setupUi();
    void loadUsers();
    void ensureDefaultAdmin();
    bool saveUsers();
    User *findUser(const QString &account);
    const User *findUser(const QString &account) const;

    QString m_dataDir;
    QString m_outDir;
    std::unique_ptr<Repository> m_repository;
    std::vector<User> m_users;
    User m_loggedInUser;
    bool m_authenticated{false};
    bool m_createdDefaultAdmin{false};

    ElaLineEdit *m_accountEdit{nullptr};
    ElaLineEdit *m_passwordEdit{nullptr};
    ElaPushButton *m_loginButton{nullptr};
    ElaPushButton *m_registerButton{nullptr};
    ElaPushButton *m_changePasswordButton{nullptr};
};
