#include "ProjectStationPositionService.h"

#include "TherionRunnerService.h"
#include "reports/TherionSqlReportDatabase.h"

#include "../core/TherionFileTypes.h"
#include "../core/TherionStationPositionExport.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QtConcurrent/QtConcurrentRun>

namespace TherionStudio
{
namespace
{
const QString kConfigFileName = QStringLiteral("station-positions.thconfig");
const QString kExportFileName = QStringLiteral("station-positions.sql");
// Far more stations and legs than any cave survey; the queries read them all.
constexpr int kMaximumRows = 10000000;

// Positions and legs do not depend on drawings, so saving one does not count.
bool sourceSavedSince(const QStringList &sourceFiles, const QDateTime &time)
{
    for (const QString &sourceFile : sourceFiles) {
        if (therionSourceDocumentTypeForFilePath(sourceFile) != TherionSourceDocumentType::TherionMap
            && QFileInfo(sourceFile).lastModified() > time) {
            return true;
        }
    }
    return false;
}
}

ProjectStationPositionService::ProjectStationPositionService(const QString &temporaryBasePath, QObject *parent)
    : QObject(parent)
    , temporaryBasePath_(temporaryBasePath)
    , runner_(new TherionRunnerService(this))
    , loadWatcher_(new QFutureWatcher<LoadResult>(this))
    , compileLoadWatcher_(new QFutureWatcher<LoadResult>(this))
{
    connect(runner_, &TherionRunnerService::runFinished, this, &ProjectStationPositionService::handleRunFinished);
    connect(runner_, &TherionRunnerService::runError, this, [this](const QString &errorText) {
        if (running_ && !runner_->isRunning() && !loadWatcher_->isRunning()) {
            finish(tr("Therion could not compute station positions: %1").arg(errorText));
        }
    });
    connect(loadWatcher_, &QFutureWatcher<LoadResult>::finished, this, &ProjectStationPositionService::handleLoadFinished);
    connect(compileLoadWatcher_, &QFutureWatcher<LoadResult>::finished, this, [this]() {
        const LoadResult result = compileLoadWatcher_->result();
        if (result.generation != generation_) {
            return;
        }
        if (!result.errorMessage.isEmpty()) {
            emit positionsFailed(result.errorMessage);
            return;
        }
        positions_ = result.positions;
        legs_ = result.legs;
        dataSource_ = DataSource::Compile;
        dataTakenAt_ = result.takenAt;
        emit positionsReady();
    });
}

ProjectStationPositionService::~ProjectStationPositionService()
{
    runner_->stop();
    loadWatcher_->waitForFinished();
    compileLoadWatcher_->waitForFinished();
}

void ProjectStationPositionService::run(const ProjectStationPositionRequest &request)
{
    if (request.configPath.isEmpty()) {
        return;
    }
    if (running_) {
        queued_ = request;
        return;
    }
    start(request);
}

void ProjectStationPositionService::cancel()
{
    ++generation_;
    queued_.reset();
    if (running_) {
        running_ = false;
        runner_->stop();
        emit runningChanged(false);
    }
}

bool ProjectStationPositionService::attemptFailed(const ProjectStationPositionRequest &request) const
{
    return attemptFailed_ && attemptKey_ == request.configPath + QLatin1Char('\n') + request.executableInput
        && !sourceSavedSince(request.sourceFiles, attemptStartedAt_);
}

void ProjectStationPositionService::loadCompileExport(const QString &sqlPath, const QDateTime &compileStartedAt)
{
    // A compile is newer than any run, so a run in progress is dropped.
    ++generation_;
    queued_.reset();
    if (running_) {
        running_ = false;
        runner_->stop();
        emit runningChanged(false);
    }
    const quint64 generation = generation_;
    compileLoadWatcher_->setFuture(QtConcurrent::run([sqlPath, generation, compileStartedAt]() {
        LoadResult result = loadPositions(sqlPath);
        result.generation = generation;
        result.takenAt = compileStartedAt;
        return result;
    }));
}

void ProjectStationPositionService::clear()
{
    ++generation_;
    queued_.reset();
    positions_.clear();
    legs_.clear();
    dataSource_ = DataSource::None;
    dataTakenAt_ = QDateTime();
    attemptKey_.clear();
    attemptStartedAt_ = QDateTime();
    attemptFailed_ = false;
    if (running_) {
        running_ = false;
        runner_->stop();
        emit runningChanged(false);
    }
}

bool ProjectStationPositionService::isRunning() const
{
    return running_;
}

ProjectStationPositionService::DataSource ProjectStationPositionService::dataSource() const
{
    return dataSource_;
}

QDateTime ProjectStationPositionService::dataTakenAt() const
{
    return dataTakenAt_;
}

bool ProjectStationPositionService::isOutOfDate(const QStringList &sourceFiles) const
{
    return dataSource_ != DataSource::None && sourceSavedSince(sourceFiles, dataTakenAt_);
}

const QHash<QString, Th2StationPosition> &ProjectStationPositionService::positions() const
{
    return positions_;
}

const QVector<Th2MeasuredLeg> &ProjectStationPositionService::legs() const
{
    return legs_;
}

void ProjectStationPositionService::start(const ProjectStationPositionRequest &request)
{
    attemptKey_ = request.configPath + QLatin1Char('\n') + request.executableInput;
    attemptStartedAt_ = QDateTime::currentDateTime();
    running_ = true;
    emit runningChanged(true);

    if (temporaryDir_ == nullptr || !temporaryDir_->isValid()) {
        temporaryDir_ = std::make_unique<QTemporaryDir>(
            QDir(temporaryBasePath_).filePath(QStringLiteral("therion-studio-positions-XXXXXX")));
    }
    if (!temporaryDir_->isValid()) {
        finish(tr("Station positions could not be computed: no temporary folder."));
        return;
    }
    QFile configFile(request.configPath);
    if (!configFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        finish(tr("Station positions could not be computed: %1 could not be read.")
                   .arg(QDir::toNativeSeparators(request.configPath)));
        return;
    }
    const QStringList sources = therionConfigSourcePaths(request.configPath, QString::fromUtf8(configFile.readAll()));
    if (sources.isEmpty()) {
        finish(tr("Station positions could not be computed: the target config has no source."));
        return;
    }
    QSaveFile exportConfig(temporaryDir_->filePath(kConfigFileName));
    if (!exportConfig.open(QIODevice::WriteOnly | QIODevice::Text)
        || exportConfig.write(therionStationPositionExportConfig(sources, kExportFileName).toUtf8()) < 0
        || !exportConfig.commit()) {
        finish(tr("Station positions could not be computed: the temporary config could not be written."));
        return;
    }
    QFile::remove(temporaryDir_->filePath(kExportFileName));
    const TherionRunnerService::StartResult result =
        runner_->start(request.executableInput, temporaryDir_->path(), {kConfigFileName});
    if (result.code == TherionRunnerService::StartCode::ExecutableNotFound) {
        finish(tr("Station positions could not be computed: Therion was not found."));
    } else if (result.code != TherionRunnerService::StartCode::Started) {
        finish(tr("Station positions could not be computed: Therion did not start."));
    }
}

void ProjectStationPositionService::handleRunFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (!running_) {
        return;
    }
    const QString exportPath = temporaryDir_->filePath(kExportFileName);
    if (exitStatus != QProcess::NormalExit || exitCode != 0 || !QFileInfo::exists(exportPath)) {
        finish(tr("Therion could not compute station positions (exit code %1); check the project in the Compiler.")
                   .arg(exitCode));
        return;
    }
    const quint64 generation = generation_;
    loadWatcher_->setFuture(QtConcurrent::run([exportPath, generation]() {
        LoadResult result = loadPositions(exportPath);
        result.generation = generation;
        return result;
    }));
}

void ProjectStationPositionService::handleLoadFinished()
{
    const LoadResult result = loadWatcher_->result();
    if (!running_ || result.generation != generation_) {
        return;
    }
    if (!result.errorMessage.isEmpty()) {
        finish(result.errorMessage);
        return;
    }
    positions_ = result.positions;
    legs_ = result.legs;
    dataSource_ = DataSource::Run;
    dataTakenAt_ = attemptStartedAt_;
    attemptFailed_ = false;
    emit positionsReady();
    finish(QString());
}

void ProjectStationPositionService::finish(const QString &errorMessage)
{
    running_ = false;
    emit runningChanged(false);
    if (!errorMessage.isEmpty()) {
        // The positions there were stay usable; only this attempt failed.
        attemptFailed_ = true;
        emit positionsFailed(errorMessage);
    }
    if (queued_.has_value()) {
        const ProjectStationPositionRequest next = *queued_;
        queued_.reset();
        run(next);
    }
}

ProjectStationPositionService::LoadResult ProjectStationPositionService::loadPositions(const QString &sqlPath)
{
    LoadResult result;
    TherionSqlReportDatabase database;
    const TherionSqlReportImportResult import = database.importFile(sqlPath);
    if (!import.success) {
        result.errorMessage = tr("Station positions could not be read: %1").arg(import.errorMessage);
        return result;
    }
    QString queryError;
    const TherionSqlReportTable stations =
        database.executeReportQuery(therionStationPositionQuery(), &queryError, kMaximumRows);
    const TherionSqlReportTable legs = queryError.isEmpty()
        ? database.executeReportQuery(therionMeasuredLegQuery(), &queryError, kMaximumRows)
        : TherionSqlReportTable();
    if (!queryError.isEmpty()) {
        result.errorMessage = tr("Station positions could not be read: %1").arg(queryError);
        return result;
    }
    result.positions = therionStationPositionsFromRows(stations.rows);
    result.legs = therionMeasuredLegsFromRows(legs.rows);
    return result;
}
}
