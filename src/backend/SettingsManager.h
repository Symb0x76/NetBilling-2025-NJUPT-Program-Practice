#pragma once

#include "ElaTheme.h"

#include <QString>

struct UiSettings
{
    ElaThemeType::ThemeMode themeMode{ElaThemeType::Light};
    bool acrylicEnabled{false};
    QString rememberedAccount;
};

UiSettings loadUiSettings(const QString &dataDir);
bool saveUiSettings(const QString &dataDir, const UiSettings &settings);
