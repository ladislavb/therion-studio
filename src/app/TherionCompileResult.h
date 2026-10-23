#pragma once

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QStringList>

namespace TherionStudio
{
// What one user compile left behind, for views that show compiled data.
struct TherionCompileResult
{
    QString configPath;
    QString workingDirectory;
    QDateTime startedAt;
    QDateTime finishedAt;
    bool succeeded = false;
    // SQL database exports written by this compile.
    QStringList databasePaths;
    // .xtherion.dat written by this compile (therion -x), or empty.
    QString xtherionDataPath;

    [[nodiscard]] bool isValid() const { return finishedAt.isValid(); }
};

// Files Therion reports as written ("writing <file> ..."), resolved against the working directory.
[[nodiscard]] QStringList therionWrittenFilePaths(const QString &standardOutput, const QString &workingDirectory);

// Arguments of a user compile, with -x added once so Therion writes .xtherion.dat.
[[nodiscard]] QStringList therionCompileArguments(const QStringList &runArguments);

// True when Therion reports writing its xtherion file.
[[nodiscard]] bool therionWroteXtherionData(const QString &standardOutput);

// True when any source file was saved after the compile started.
[[nodiscard]] bool therionCompileResultIsOutOfDate(const TherionCompileResult &result, const QStringList &sourceFiles);

// Records the user's compiles and keeps the latest finished one.
class TherionCompileResultRecorder final : public QObject
{
    Q_OBJECT

public:
    explicit TherionCompileResultRecorder(QObject *parent = nullptr);

    void begin(const QString &configPath, const QString &workingDirectory);
    void appendStandardOutput(const QString &output);
    void finish(bool succeeded);
    // The run did not complete; the latest result is kept.
    void abandon();
    // The project closed or changed.
    void clear();

    [[nodiscard]] const TherionCompileResult &latest() const;

signals:
    void latestChanged();

private:
    bool active_ = false;
    TherionCompileResult pending_;
    QString pendingOutput_;
    TherionCompileResult latest_;
};
}
