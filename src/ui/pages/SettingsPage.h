#pragma once

#include "ui/pages/BasePage.h"

class ElaToggleSwitch;
class ElaPushButton;

class SettingsPage : public BasePage
{
    Q_OBJECT

public:
    explicit SettingsPage(QWidget *parent = nullptr);

    void setDarkModeChecked(bool checked);
    void setAcrylicChecked(bool checked);

Q_SIGNALS:
    void darkModeToggled(bool enabled);
    void acrylicToggled(bool enabled);
    void changePasswordRequested();
    void switchAccountRequested();
    void backupRequested();
    void restoreRequested();

public:
    void setDataManagementVisible(bool visible);

private:
    ElaToggleSwitch *m_darkModeSwitch{nullptr};
    ElaToggleSwitch *m_acrylicSwitch{nullptr};
    ElaPushButton *m_changePasswordButton{nullptr};
    ElaPushButton *m_switchAccountButton{nullptr};
    QWidget *m_backupRow{nullptr};
    ElaPushButton *m_backupButton{nullptr};
    ElaPushButton *m_restoreButton{nullptr};
};
