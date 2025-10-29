#include "backend/SettingsManager.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

namespace
{
    QString settingsFilePath(const QString &dataDir)
    {
        return QDir(dataDir).filePath(QStringLiteral("settings.json"));
    }

    QString themeModeToString(ElaThemeType::ThemeMode mode)
    {
        return mode == ElaThemeType::Dark ? QStringLiteral("dark") : QStringLiteral("light");
    }

    ElaThemeType::ThemeMode themeModeFromValue(const QJsonValue &value)
    {
        if (value.isString())
        {
            const auto text = value.toString();
            if (text.compare(QStringLiteral("dark"), Qt::CaseInsensitive) == 0)
                return ElaThemeType::Dark;
            if (text.compare(QStringLiteral("light"), Qt::CaseInsensitive) == 0)
                return ElaThemeType::Light;
        }
        if (value.isDouble())
        {
            const auto numeric = static_cast<int>(value.toDouble());
            if (numeric == static_cast<int>(ElaThemeType::Dark))
                return ElaThemeType::Dark;
        }
        return ElaThemeType::Light;
    }
} // namespace

UiSettings loadUiSettings(const QString &dataDir)
{
    UiSettings settings;
    QFile file(settingsFilePath(dataDir));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return settings;
    }

    const auto document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject())
    {
        return settings;
    }

    const QJsonObject object = document.object();
    settings.themeMode = themeModeFromValue(object.value(QStringLiteral("themeMode")));
    settings.acrylicEnabled = object.value(QStringLiteral("acrylicEnabled")).toBool(false);
    settings.rememberedAccount = object.value(QStringLiteral("lastAccount")).toString();
    return settings;
}

bool saveUiSettings(const QString &dataDir, const UiSettings &settings)
{
    QDir dir(dataDir);
    if (!dir.exists())
    {
        if (!dir.mkpath(QStringLiteral(".")))
            return false;
    }

    QFile file(settingsFilePath(dir.absolutePath()));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        return false;
    }

    QJsonObject object;
    object.insert(QStringLiteral("themeMode"), themeModeToString(settings.themeMode));
    object.insert(QStringLiteral("acrylicEnabled"), settings.acrylicEnabled);
    if (!settings.rememberedAccount.isEmpty())
        object.insert(QStringLiteral("lastAccount"), settings.rememberedAccount);

    const QJsonDocument document(object);
    const auto bytes = document.toJson(QJsonDocument::Indented);
    const auto written = file.write(bytes);
    return written == bytes.size();
}
