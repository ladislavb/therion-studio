#include "TherionCompileResult.h"

#include "TherionRunnerOutputLinker.h"

#include "../core/TherionFileTypes.h"

#include <QFileInfo>
#include <QRegularExpression>

namespace TherionStudio
{
namespace
{
const QString kXtherionDataFileName = QStringLiteral(".xtherion.dat");
const QString kXtherionFileMessage = QStringLiteral("xtherion file");
}

QStringList therionWrittenFilePaths(const QString &standardOutput, const QString &workingDirectory)
{
    // Therion prints "writing <file> ... " (or "writing <file>" in debug builds) before each output file.
    static const QRegularExpression writingPattern(
        QStringLiteral(R"(^writing (.+?)(?:\s+\.\.\..*)?$)"),
        QRegularExpression::MultilineOption);
    QStringList paths;
    QString text = standardOutput;
    text.remove(QLatin1Char('\r'));
    QRegularExpressionMatchIterator matches = writingPattern.globalMatch(text);
    while (matches.hasNext()) {
        const QString fileName = matches.next().captured(1).trimmed();
        if (fileName.isEmpty() || fileName.startsWith(kXtherionFileMessage)) {
            continue;
        }
        const QString path = TherionRunnerOutputLinker::absolutePathForCompilerPath(fileName, workingDirectory);
        if (!path.isEmpty() && !paths.contains(path)) {
            paths.append(path);
        }
    }
    return paths;
}

bool therionWroteXtherionData(const QString &standardOutput)
{
    static const QRegularExpression xtherionPattern(
        QStringLiteral(R"(^writing xtherion file\b)"),
        QRegularExpression::MultilineOption);
    return xtherionPattern.match(standardOutput).hasMatch();
}

bool therionCompileResultIsOutOfDate(const TherionCompileResult &result, const QStringList &sourceFiles)
{
    if (!result.isValid()) {
        return true;
    }
    for (const QString &sourceFile : sourceFiles) {
        if (QFileInfo(sourceFile).lastModified() > result.startedAt) {
            return true;
        }
    }
    return false;
}

TherionCompileResultRecorder::TherionCompileResultRecorder(QObject *parent)
    : QObject(parent)
{
}

void TherionCompileResultRecorder::begin(const QString &configPath, const QString &workingDirectory)
{
    active_ = true;
    pending_ = TherionCompileResult();
    pending_.configPath = configPath;
    pending_.workingDirectory = workingDirectory;
    pending_.startedAt = QDateTime::currentDateTime();
    pendingOutput_.clear();
}

void TherionCompileResultRecorder::appendStandardOutput(const QString &output)
{
    if (active_) {
        pendingOutput_ += output;
    }
}

void TherionCompileResultRecorder::finish(bool succeeded)
{
    if (!active_) {
        return;
    }
    active_ = false;
    pending_.finishedAt = QDateTime::currentDateTime();
    pending_.succeeded = succeeded;
    for (const QString &path : therionWrittenFilePaths(pendingOutput_, pending_.workingDirectory)) {
        if (isTherionDatabaseOutputFilePath(path) && QFileInfo::exists(path)) {
            pending_.databasePaths.append(path);
        }
    }
    const QString xtherionDataPath =
        TherionRunnerOutputLinker::absolutePathForCompilerPath(kXtherionDataFileName, pending_.workingDirectory);
    if (therionWroteXtherionData(pendingOutput_) && QFileInfo::exists(xtherionDataPath)) {
        pending_.xtherionDataPath = xtherionDataPath;
    }
    pendingOutput_.clear();
    latest_ = pending_;
    emit latestChanged();
}

void TherionCompileResultRecorder::abandon()
{
    active_ = false;
    pendingOutput_.clear();
}

void TherionCompileResultRecorder::clear()
{
    abandon();
    const bool hadResult = latest_.isValid();
    latest_ = TherionCompileResult();
    if (hadResult) {
        emit latestChanged();
    }
}

const TherionCompileResult &TherionCompileResultRecorder::latest() const
{
    return latest_;
}
}
