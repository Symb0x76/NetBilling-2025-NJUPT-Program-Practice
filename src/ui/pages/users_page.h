#pragma once

#include "ui/pages/base_page.h"

#include <memory>
#include <QStringList>
#include <vector>

class ElaTableView;
class ElaPushButton;
class ElaLineEdit;
class ElaComboBox;
class UsersFilterProxyModel;
class QStandardItemModel;

struct User;

class UsersPage : public BasePage
{
    Q_OBJECT

public:
    explicit UsersPage(QWidget* parent = nullptr);

    void setUsers(const std::vector<User>& users);
    QStringList selectedAccounts() const;
    void clearSelection();

Q_SIGNALS:
    void requestCreateUser();
    void requestEditUser(const QString& account);
    void requestDeleteUsers(const QStringList& accounts);
    void requestReloadUsers();
    void requestSaveUsers();

private:
    void setupToolbar();
    void setupTable();
    void applyFilter(const QString& text);

    ElaLineEdit* m_searchEdit{nullptr};
    ElaComboBox* m_planFilterCombo{nullptr};
    ElaPushButton* m_addButton{nullptr};
    ElaPushButton* m_editButton{nullptr};
    ElaPushButton* m_deleteButton{nullptr};
    ElaPushButton* m_reloadButton{nullptr};
    ElaPushButton* m_saveButton{nullptr};
    ElaTableView* m_table{nullptr};
    std::unique_ptr<QStandardItemModel> m_model;
    std::unique_ptr<UsersFilterProxyModel> m_proxyModel;
};
