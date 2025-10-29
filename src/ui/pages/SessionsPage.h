#pragma once

#include "ui/pages/BasePage.h"

#include <memory>

#include <QSortFilterProxyModel>
#include <QStandardItemModel>

#include <QHash>
#include <QList>
#include <QString>
#include <vector>

class ElaTableView;
class ElaPushButton;
class ElaLineEdit;
class ElaComboBox;
struct Session;

class SessionsPage : public BasePage
{
    Q_OBJECT

public:
    explicit SessionsPage(QWidget *parent = nullptr);

    void setSessions(const std::vector<Session> &sessions, const QHash<QString, QString> &accountNames);
    void setAdminMode(bool adminMode);
    void setRestrictedAccount(const QString &account);
    QList<Session> selectedSessions() const;
    void clearSelection();

Q_SIGNALS:
    void requestCreateSession();
    void requestEditSession(const Session &session);
    void requestDeleteSessions(const QList<Session> &sessions);
    void requestReloadSessions();
    void requestSaveSessions();
    void requestGenerateRandomSessions();

private:
    void setupToolbar();
    void setupTable();
    void reloadPageData() override;
    void applyFilter(const QString &text);

    ElaLineEdit *m_searchEdit{nullptr};
    ElaComboBox *m_scopeFilterCombo{nullptr};
    ElaPushButton *m_addButton{nullptr};
    ElaPushButton *m_editButton{nullptr};
    ElaPushButton *m_deleteButton{nullptr};
    ElaPushButton *m_reloadButton{nullptr};
    ElaPushButton *m_saveButton{nullptr};
    ElaPushButton *m_generateButton{nullptr};
    ElaTableView *m_table{nullptr};
    std::unique_ptr<QStandardItemModel> m_model;
    std::unique_ptr<QSortFilterProxyModel> m_proxyModel;
    bool m_adminMode{true};
    QString m_restrictedAccount;
    QHash<QString, QString> m_accountNames;
};
