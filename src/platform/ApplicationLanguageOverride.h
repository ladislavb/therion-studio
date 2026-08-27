#pragma once

#include <QString>

namespace TherionStudio::Platform
{

inline QString normalizeApplicationLanguageSetting(const QString &language)
{
    QString normalized = language.trimmed().toLower();
    normalized.replace(QChar(u'_'), QChar(u'-'));

    if (normalized == QStringLiteral("en") || normalized.startsWith(QStringLiteral("en-"))) {
        return QStringLiteral("en");
    }
    if (normalized == QStringLiteral("cs") || normalized.startsWith(QStringLiteral("cs-"))) {
        return QStringLiteral("cs");
    }
    if (normalized == QStringLiteral("fr") || normalized.startsWith(QStringLiteral("fr-"))) {
        return QStringLiteral("fr");
    }
    if (normalized == QStringLiteral("sk") || normalized.startsWith(QStringLiteral("sk-"))) {
        return QStringLiteral("sk");
    }
    return QStringLiteral("system");
}

QString applicationLanguageOverride();
void setApplicationLanguageOverride(const QString &language);

} // namespace TherionStudio::Platform
