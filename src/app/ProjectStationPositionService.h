#pragma once

#include "../core/Th2CentrelineAlignment.h"

#include <QDateTime>
#include <QFutureWatcher>
#include <QHash>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>

#include <memory>
#include <optional>

class QTemporaryDir;

namespace TherionStudio
{
class TherionRunnerService;

struct ProjectStationPositionRequest
{
    QString executableInput;
    QString configPath;
    // Project source files; the export is redone when any of them is newer than it, drawings (.th2) apart.
    QStringList sourceFiles;
};

// Measured legs and computed station positions from Therion for the whole project, used to place joined drawings.
// They come from the database export of the user's compile when there is one. Otherwise run() runs Therion in the
// background on its own runner (the Compiler panel stays free) with a temporary config that only exports the
// database, in a temporary folder outside the project. Whether to run is the caller's choice. One run at a time: a
// run asked for while running is queued once.
class ProjectStationPositionService final : public QObject
{
    Q_OBJECT

public:
    enum class DataSource
    {
        None,
        Compile,
        Run,
    };

    // `temporaryBasePath` holds the temporary folder (chosen at the composition boundary).
    explicit ProjectStationPositionService(const QString &temporaryBasePath, QObject *parent = nullptr);
    ~ProjectStationPositionService() override;

    void run(const ProjectStationPositionRequest &request);
    // Stops a run; the positions there were are kept.
    void cancel();
    // The last run for this config and executable failed and no source file other than a drawing has changed since,
    // so another run would fail the same way.
    [[nodiscard]] bool attemptFailed(const ProjectStationPositionRequest &request) const;
    // Reads positions from the database export of the user's compile, replacing any from a run.
    void loadCompileExport(const QString &sqlPath, const QDateTime &compileStartedAt);
    // Forgets positions and stops a run, for example when the project closes.
    void clear();

    [[nodiscard]] bool isRunning() const;
    [[nodiscard]] DataSource dataSource() const;
    // When the compile or run that produced the positions started.
    [[nodiscard]] QDateTime dataTakenAt() const;
    // A source file other than a drawing was saved after the positions were taken.
    [[nodiscard]] bool isOutOfDate(const QStringList &sourceFiles) const;
    [[nodiscard]] const QHash<QString, Th2StationPosition> &positions() const;
    // From the same export as positions(), so both are present or both empty.
    [[nodiscard]] const QVector<Th2MeasuredLeg> &legs() const;

signals:
    void runningChanged(bool running);
    void positionsReady();
    void positionsFailed(const QString &message);

private:
    struct LoadResult
    {
        QHash<QString, Th2StationPosition> positions;
        QVector<Th2MeasuredLeg> legs;
        QString errorMessage;
        quint64 generation = 0;
        QDateTime takenAt;
    };

    void start(const ProjectStationPositionRequest &request);
    void handleRunFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void handleLoadFinished();
    void finish(const QString &errorMessage);
    [[nodiscard]] static LoadResult loadPositions(const QString &sqlPath);

    QString temporaryBasePath_;
    std::unique_ptr<QTemporaryDir> temporaryDir_;
    TherionRunnerService *runner_ = nullptr;
    QFutureWatcher<LoadResult> *loadWatcher_ = nullptr;
    QFutureWatcher<LoadResult> *compileLoadWatcher_ = nullptr;
    std::optional<ProjectStationPositionRequest> queued_;
    // Config and executable of the last attempt, and when it started; sources newer than that make it stale.
    QString attemptKey_;
    QDateTime attemptStartedAt_;
    bool attemptFailed_ = false;
    QHash<QString, Th2StationPosition> positions_;
    QVector<Th2MeasuredLeg> legs_;
    DataSource dataSource_ = DataSource::None;
    QDateTime dataTakenAt_;
    bool running_ = false;
    quint64 generation_ = 0;
};
}
