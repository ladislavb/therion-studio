#include "ProjectAltitudeService.h"

#include "../core/ThreeDViewerLoxLoader.h"

#include <QFileInfo>
#include <QtConcurrent/QtConcurrentRun>

namespace TherionStudio
{
ProjectAltitudeService::ProjectAltitudeService(QObject *parent)
    : QObject(parent)
{
    connect(&watcher_, &QFutureWatcher<Result>::finished, this, [this]() {
        const Result result = watcher_.result();
        if (enabled_ && result.generation == generation_) {
            stations_ = result.stations;
            loadedKey_ = result.key;
            emit stationsChanged();
        }
        startLoad();
    });
}

void ProjectAltitudeService::setEnabled(bool enabled)
{
    if (enabled_ == enabled) {
        return;
    }
    enabled_ = enabled;
    ++generation_;
    stations_ = {};
    stations_.outputCount = outputCount_;
    if (outputCount_ > 1) {
        stations_.sourceState = ScrapAltitudeStations::SourceState::AmbiguousOutputs;
    }
    loadedKey_.clear();
    emit stationsChanged();
    startLoad();
}

void ProjectAltitudeService::acceptOutputs(const ProjectOutputsScanner::Result &result)
{
    QStringList paths;
    if (result.errorMessage.isEmpty()) {
        for (const auto &artifact : result.artifacts) {
            if (artifact.filePath.endsWith(QStringLiteral(".lox"), Qt::CaseInsensitive)) {
                paths.append(artifact.filePath);
            }
        }
    }
    paths.removeDuplicates();
    const QString path = paths.size() == 1 ? paths.first() : QString();
    const QFileInfo info(path);
    const QString key = result.projectRootPath + QLatin1Char('|') + path
        + QLatin1Char('|') + QString::number(paths.size())
        + QLatin1Char('|') + QString::number(info.size())
        + QLatin1Char('|') + QString::number(info.lastModified().toMSecsSinceEpoch());
    if (key == requestedKey_) {
        return;
    }
    ++generation_;
    root_ = result.projectRootPath;
    outputCount_ = paths.size();
    requestedPath_ = path;
    requestedKey_ = key;
    loadedKey_.clear();
    stations_ = {};
    stations_.outputCount = outputCount_;
    if (outputCount_ > 1) {
        stations_.sourceState = ScrapAltitudeStations::SourceState::AmbiguousOutputs;
    }
    emit stationsChanged();
    startLoad();
}

void ProjectAltitudeService::startLoad()
{
    if (!enabled_ || watcher_.isRunning() || requestedKey_.isEmpty()
        || loadedKey_ == requestedKey_) {
        return;
    }
    const QString path = requestedPath_;
    if (path.isEmpty()) {
        loadedKey_ = requestedKey_;
        return;
    }
    const QString key = requestedKey_;
    const quint64 generation = generation_;
    watcher_.setFuture(QtConcurrent::run([path, key, generation]() {
        Result result;
        result.key = key;
        result.generation = generation;
        result.stations.outputCount = 1;
        result.stations.sourceState = ScrapAltitudeStations::SourceState::LoadFailed;
        const QFileInfo info(path);
        if (!path.isEmpty() && info.isFile()) {
            const auto loaded = ThreeDViewerLoxLoader().loadStationMetadataFile(path);
            if (loaded.error.isEmpty()) {
                result.stations = scrapAltitudeStations(loaded.scene);
                result.stations.outputCount = 1;
            }
        }
        return result;
    }));
}

const ScrapAltitudeStations &ProjectAltitudeService::stations() const
{
    return stations_;
}

QString ProjectAltitudeService::projectRootPath() const
{
    return root_;
}
}
