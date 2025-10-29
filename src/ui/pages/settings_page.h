#pragma once

#include "ui/pages/base_page.h"

class ElaToggleSwitch;
class ElaPushButton;
class QLabel;
class QPixmap;

class SettingsPage : public BasePage
{
    Q_OBJECT

public:
    explicit SettingsPage(QWidget *parent = nullptr);

    void setDarkModeChecked(bool checked);
    void setAcrylicChecked(bool checked);
    void setAvatarPreview(const QPixmap &pixmap);

Q_SIGNALS:
    void darkModeToggled(bool enabled);
    void acrylicToggled(bool enabled);
    void avatarChangeRequested();

private:
    ElaToggleSwitch *m_darkModeSwitch{nullptr};
    ElaToggleSwitch *m_acrylicSwitch{nullptr};
    QLabel *m_avatarPreview{nullptr};
    ElaPushButton *m_changeAvatarButton{nullptr};
    int m_avatarPreviewSize{72};
};
