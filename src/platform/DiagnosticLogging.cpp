#include "DiagnosticLogging.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QMutex>
#include <QMutexLocker>
#include <QDebug>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTextStream>
#include <QtGlobal>

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
    QMutex mutex;
    QtMessageHandler previousHandler = nullptr;
};

std::unique_ptr<DiagnosticLogState> &diagnosticLogState()
{
    static std::unique_ptr<DiagnosticLogState> state;
    return state;
}

QString defaultDiagnosticLogPath()
{
    QString directory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (directory.isEmpty()) {
        directory = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    }
    if (directory.isEmpty()) {
        return {};
    }
    return QDir(directory).filePath(QStringLiteral("therion-studio.log"));
}

QString diagnosticLogPath()
{
    const QString enabled = QString::fromLocal8Bit(qgetenv("THERION_STUDIO_ENABLE_LOG")).trimmed().toLower();
    if (enabled != QStringLiteral("1")
        && enabled != QStringLiteral("true")
        && enabled != QStringLiteral("yes")
        && enabled != QStringLiteral("on")) {
        return {};
    }
    if (qEnvironmentVariableIsSet("THERION_STUDIO_LOG_FILE")) {
        return QString::fromLocal8Bit(qgetenv("THERION_STUDIO_LOG_FILE"));
    }
    return defaultDiagnosticLogPath();
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

void initializeDiagnosticLogging()
{
    const QString filePath = diagnosticLogPath();
    if (filePath.isEmpty()) {
        return;
    }

    auto state = std::make_unique<DiagnosticLogState>();
    state->file.setFileName(filePath);
    const QFileInfo fileInfo(state->file);
    if (!fileInfo.path().isEmpty()) {
        QDir().mkpath(fileInfo.path());
    }
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
