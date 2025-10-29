#pragma once

#include "ElaTheme.h"

#include <QString>

struct UiSettings
{
    ElaThemeType::ThemeMode themeMode{ElaThemeType::Light};
    bool acrylicEnabled{false};
    QString avatarFileName;
    QString rememberedAccount;
};

UiSettings loadUiSettings(const QString &dataDir);
bool saveUiSettings(const QString &dataDir, const UiSettings &settings);
