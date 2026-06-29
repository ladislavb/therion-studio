#include "DiagnosticLogging.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileInfoList>
#include <QIODevice>
#include <QMutex>
#include <QMutexLocker>
#include <QObject>
#include <QDebug>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTextStream>
#include <QtGlobal>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <memory>

namespace TherionStudio
{
namespace
{
#ifndef THERION_STUDIO_VERSION_STRING
#define THERION_STUDIO_VERSION_STRING "unknown"
#endif

#ifndef THERION_STUDIO_PACKAGE_LABEL_STRING
#define THERION_STUDIO_PACKAGE_LABEL_STRING THERION_STUDIO_VERSION_STRING
#endif

struct DiagnosticLogState
{
    QFile file;
    QString filePath;
    QMutex mutex;
    QtMessageHandler previousHandler = nullptr;
};

constexpr qint64 kMaximumDiagnosticLogFileBytes = 5 * 1024 * 1024;
constexpr int kMaximumDiagnosticLogFiles = 5;
const auto kDiagnosticLogFileName = QStringLiteral("therion-studio.log");
const auto kDiagnosticLogArchivePrefix = QStringLiteral("therion-studio-");
const auto kDiagnosticLogArchiveSuffix = QStringLiteral(".log");

std::unique_ptr<DiagnosticLogState> &diagnosticLogState()
{
    static std::unique_ptr<DiagnosticLogState> state;
    return state;
}

QString defaultDiagnosticLogDirectoryPath()
{
    QString directory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (directory.isEmpty()) {
        directory = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    }
    if (directory.isEmpty()) {
        return {};
    }
    return QDir(directory).filePath(QStringLiteral("logs"));
}

QString defaultDiagnosticLogFilePath()
{
    const QString directory = defaultDiagnosticLogDirectoryPath();
    return directory.isEmpty() ? QString() : QDir(directory).filePath(kDiagnosticLogFileName);
}

bool diagnosticLoggingEnvironmentEnabled()
{
    const QString enabled = QString::fromLocal8Bit(qgetenv("THERION_STUDIO_ENABLE_LOG")).trimmed().toLower();
    return enabled == QStringLiteral("1")
        || enabled == QStringLiteral("true")
        || enabled == QStringLiteral("yes")
        || enabled == QStringLiteral("on");
}

QString diagnosticLogPath(bool enableFromPreference)
{
    if (!diagnosticLoggingEnvironmentEnabled() && !enableFromPreference) {
        return {};
    }
    if (qEnvironmentVariableIsSet("THERION_STUDIO_LOG_FILE")) {
        return QString::fromLocal8Bit(qgetenv("THERION_STUDIO_LOG_FILE"));
    }
    return defaultDiagnosticLogFilePath();
}

QFileInfoList diagnosticLogFiles()
{
    const QString directoryPath = defaultDiagnosticLogDirectoryPath();
    if (directoryPath.isEmpty()) {
        return {};
    }

    QDir directory(directoryPath);
    const QStringList filters = {
        kDiagnosticLogFileName,
        kDiagnosticLogArchivePrefix + QStringLiteral("*") + kDiagnosticLogArchiveSuffix,
    };
    return directory.entryInfoList(filters, QDir::Files, QDir::Time);
}

void pruneDiagnosticLogs()
{
    QFileInfoList files = diagnosticLogFiles();
    std::sort(files.begin(), files.end(), [](const QFileInfo &left, const QFileInfo &right) {
        return left.lastModified() > right.lastModified();
    });
    for (int index = kMaximumDiagnosticLogFiles; index < files.size(); ++index) {
        QFile::remove(files.at(index).absoluteFilePath());
    }
}

QString archivedDiagnosticLogPath(const QString &filePath)
{
    const QFileInfo fileInfo(filePath);
    const QString timestamp = QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd-HHmmss-zzz"));
    return QDir(fileInfo.path()).filePath(kDiagnosticLogArchivePrefix + timestamp + kDiagnosticLogArchiveSuffix);
}

void rotateExistingDiagnosticLog(const QString &filePath)
{
    const QFileInfo fileInfo(filePath);
    if (!fileInfo.exists() || fileInfo.size() <= 0) {
        return;
    }

    QDir().mkpath(fileInfo.path());
    const QString archivePath = archivedDiagnosticLogPath(filePath);
    if (!QFile::rename(filePath, archivePath)) {
        QFile::remove(filePath);
    }
    pruneDiagnosticLogs();
}

void rotateOpenDiagnosticLogLocked(DiagnosticLogState &state)
{
    if (!state.file.isOpen() || state.file.size() < kMaximumDiagnosticLogFileBytes) {
        return;
    }

    state.file.flush();
    state.file.close();
    const QString archivePath = archivedDiagnosticLogPath(state.filePath);
    if (!QFile::rename(state.filePath, archivePath)) {
        QFile::remove(state.filePath);
    }
    state.file.setFileName(state.filePath);
    if (!state.file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        std::fprintf(stderr,
                     "Therion Studio diagnostic log could not be reopened after rotation: %s\n",
                     qPrintable(state.filePath));
    }
    pruneDiagnosticLogs();
}

QString messageTypeName(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg:
        return QStringLiteral("debug");
    case QtInfoMsg:
        return QStringLiteral("info");
    case QtWarningMsg:
        return QStringLiteral("warning");
    case QtCriticalMsg:
        return QStringLiteral("critical");
    case QtFatalMsg:
        return QStringLiteral("fatal");
    }
    return QStringLiteral("message");
}

QString buildString(const char *value)
{
    return QString::fromUtf8(value);
}

QString diagnosticStartupSummary()
{
    const QString platform = QStringLiteral("%1 (%2)")
                                 .arg(QSysInfo::prettyProductName(),
                                      QSysInfo::currentCpuArchitecture());
    return QStringLiteral("Version: %1\nBuild: %2\nQt: %3\nPlatform: %4")
        .arg(buildString(THERION_STUDIO_VERSION_STRING),
             buildString(THERION_STUDIO_PACKAGE_LABEL_STRING),
             QString::fromLatin1(QT_VERSION_STR),
             platform);
}

void diagnosticMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    if (auto &state = diagnosticLogState()) {
        QMutexLocker locker(&state->mutex);
        rotateOpenDiagnosticLogLocked(*state);
        if (!state->file.isOpen()) {
            return;
        }
        QTextStream stream(&state->file);
        stream << QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)
               << " [" << messageTypeName(type) << "]";
        if (context.category != nullptr && context.category[0] != '\0') {
            stream << " " << context.category;
        }
        if (context.file != nullptr && context.file[0] != '\0') {
            stream << " (" << context.file << ':' << context.line << ')';
        }
        stream << ' ' << message << '\n';
        stream.flush();

        if (state->previousHandler != nullptr) {
            state->previousHandler(type, context, message);
        } else {
            std::fprintf(stderr, "%s\n", qPrintable(message));
        }
        if (type == QtFatalMsg) {
            std::abort();
        }
    }
}
} // namespace

QString diagnosticLogDirectoryPath()
{
    return defaultDiagnosticLogDirectoryPath();
}

QString diagnosticLogFilePath()
{
    return defaultDiagnosticLogFilePath();
}

bool clearDiagnosticLogs(QString *errorMessage)
{
    const QFileInfoList files = diagnosticLogFiles();
    if (files.isEmpty()) {
        return true;
    }

    QString activeLogPath;
    if (auto &state = diagnosticLogState()) {
        QMutexLocker locker(&state->mutex);
        activeLogPath = QFileInfo(state->filePath).absoluteFilePath();
        if (state->file.isOpen()) {
            state->file.resize(0);
            state->file.seek(0);
        }
    }

    for (const QFileInfo &fileInfo : files) {
        if (!activeLogPath.isEmpty() && fileInfo.absoluteFilePath() == activeLogPath) {
            continue;
        }
        if (!QFile::remove(fileInfo.absoluteFilePath())) {
            if (errorMessage != nullptr) {
                *errorMessage = QObject::tr("Could not remove log file `%1`.").arg(fileInfo.absoluteFilePath());
            }
            return false;
        }
    }
    return true;
}

void initializeDiagnosticLogging(bool enableFromPreference)
{
    if (enableFromPreference && !diagnosticLoggingEnvironmentEnabled()) {
        qputenv("THERION_STUDIO_ENABLE_LOG", "1");
    }

    const QString filePath = diagnosticLogPath(enableFromPreference);
    if (filePath.isEmpty()) {
        pruneDiagnosticLogs();
        return;
    }

    auto state = std::make_unique<DiagnosticLogState>();
    state->file.setFileName(filePath);
    state->filePath = filePath;
    const QFileInfo fileInfo(state->file);
    if (!fileInfo.path().isEmpty()) {
        QDir().mkpath(fileInfo.path());
    }
    rotateExistingDiagnosticLog(filePath);
    if (!state->file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        std::fprintf(stderr,
                     "Therion Studio diagnostic log could not be opened: %s\n",
                     qPrintable(filePath));
        return;
    }

    auto &globalState = diagnosticLogState();
    globalState = std::move(state);
    globalState->previousHandler = qInstallMessageHandler(diagnosticMessageHandler);
    qInfo("Therion Studio diagnostic logging enabled: %s", qPrintable(filePath));
    qInfo().noquote() << diagnosticStartupSummary();
}

} // namespace TherionStudio
