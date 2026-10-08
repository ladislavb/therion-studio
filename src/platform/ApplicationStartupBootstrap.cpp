#include "ApplicationStartupBootstrap.h"
#include "ApplicationIdentity.h"
#include "ApplicationLanguageOverride.h"

#include <QApplication>
#include <QIcon>
#include <QLibraryInfo>
#include <QLocale>
#include <QSettings>
#include <QStringList>
#include <QTranslator>

#include <array>
#include <memory>
#include <vector>

namespace
{
constexpr auto kApplicationLanguageKey = "settings/applicationLanguage";
constexpr auto kLegacySettingsOrganizationDomain = "therionstudio.example";
constexpr auto kLegacySettingsApplicationName = "Therion Studio";
constexpr auto kSettingsMigrationMarkerKey = "settings/migratedFromTherionstudioExample";

void applyApplicationIdentity(QApplication &application)
{
    const TherionStudio::Platform::ApplicationIdentity &identity =
        TherionStudio::Platform::applicationIdentity();
    application.setApplicationName(identity.applicationName);
    application.setApplicationDisplayName(identity.applicationDisplayName);
    application.setOrganizationName(identity.organizationName);
    application.setOrganizationDomain(identity.organizationDomain);
    application.setWindowIcon(QIcon(QStringLiteral(":/resources/app/app-icon.png")));
}

void migrateLegacySettingsNamespace()
{
    QSettings currentSettings;
    if (currentSettings.value(QString::fromLatin1(kSettingsMigrationMarkerKey), false).toBool()) {
        return;
    }

    const QSettings legacySettings(QSettings::NativeFormat,
                                   QSettings::UserScope,
                                   QString::fromLatin1(kLegacySettingsOrganizationDomain),
                                   QString::fromLatin1(kLegacySettingsApplicationName));
    const QStringList legacyKeys = legacySettings.allKeys();
    if (legacyKeys.isEmpty()) {
        return;
    }

    for (const QString &key : legacyKeys) {
        if (currentSettings.contains(key)) {
            continue;
        }
        currentSettings.setValue(key, legacySettings.value(key));
    }

    currentSettings.setValue(QString::fromLatin1(kSettingsMigrationMarkerKey), true);
}

QLocale preferredApplicationLocale()
{
    QString language = TherionStudio::Platform::applicationLanguageOverride();
    if (language == QStringLiteral("system")) {
        language = QSettings()
            .value(QString::fromLatin1(kApplicationLanguageKey), QStringLiteral("system"))
            .toString();
    }
    language = TherionStudio::Platform::normalizeApplicationLanguageSetting(language);
    if (language == QStringLiteral("cs") || language == QStringLiteral("fr")
        || language == QStringLiteral("sk")) {
        return QLocale(language);
    }
    if (language == QStringLiteral("en")) {
        return QLocale::c();
    }
    return QLocale();
}

std::unique_ptr<QTranslator> loadTranslator(const QLocale &locale,
                                            const QString &baseName,
                                            const QString &directory)
{
    auto translator = std::make_unique<QTranslator>();
    if (!translator->load(locale, baseName, QStringLiteral("_"), directory)) {
        return nullptr;
    }
    return translator;
}

std::vector<std::unique_ptr<QTranslator>> installApplicationTranslators(
    QApplication &application)
{
    const QLocale locale = preferredApplicationLocale();
    if (locale.language() == QLocale::C || locale.language() == QLocale::English) {
        return {};
    }

    std::vector<std::unique_ptr<QTranslator>> translators;

    const QString qtTranslationsPath = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
    constexpr std::array<const char *, 2> qtTranslationCatalogs = {"qtbase", "qt"};
    for (const char *catalog : qtTranslationCatalogs) {
        auto translator = loadTranslator(locale,
                                         QString::fromLatin1(catalog),
                                         qtTranslationsPath);
        if (translator == nullptr) {
            continue;
        }
        application.installTranslator(translator.get());
        translators.push_back(std::move(translator));
    }

    auto applicationTranslator = loadTranslator(locale,
                                                QStringLiteral("therion_studio"),
                                                QStringLiteral(":/i18n"));
    if (applicationTranslator != nullptr) {
        application.installTranslator(applicationTranslator.get());
        translators.push_back(std::move(applicationTranslator));
    }

    return translators;
}
} // namespace

namespace TherionStudio
{

ApplicationStartupState initializeApplicationStartup(QApplication &application)
{
    applyApplicationIdentity(application);
    migrateLegacySettingsNamespace();

    ApplicationStartupState state;
    state.translators = installApplicationTranslators(application);
    return state;
}

} // namespace TherionStudio
