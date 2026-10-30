#pragma once

#include "ProjectOutputsScanner.h"
#include "../core/ScrapAltitudeModel.h"

#include <QFutureWatcher>
#include <QObject>

namespace TherionStudio
{
class ProjectAltitudeService final : public QObject
{
    Q_OBJECT
public:
    explicit ProjectAltitudeService(QObject *parent = nullptr);
    void setEnabled(bool enabled);
    void acceptOutputs(const ProjectOutputsScanner::Result &result);
    const ScrapAltitudeStations &stations() const;
    QString projectRootPath() const;

signals:
    void stationsChanged();

private:
    struct Result
    {
        ScrapAltitudeStations stations;
        QString key;
        quint64 generation = 0;
    };
    void startLoad();
    bool enabled_ = false;
    quint64 generation_ = 0;
    QString root_;
    QString requestedPath_;
    QString requestedKey_;
    QString loadedKey_;
    int outputCount_ = 0;
    ScrapAltitudeStations stations_;
    QFutureWatcher<Result> watcher_;
};
}
