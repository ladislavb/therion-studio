#pragma once

#include <QObject>
#include <QString>
#include <QVector>

class QTimer;

template <typename T>
class QFutureWatcher;

namespace TherionStudio
{

class ProjectOutputsScanner final : public QObject
{
    Q_OBJECT

public:
    enum class ArtifactKind
    {
        Model,
        MapAtlas,
        Database
    };

    struct Artifact
    {
        QString filePath;
        QString relativePath;
        QString displayName;
        ArtifactKind kind = ArtifactKind::Model;
    };

    struct Result
    {
        quint64 generation = 0;
        QString projectRootPath;
        QString errorMessage;
        QVector<Artifact> artifacts;
    };

    explicit ProjectOutputsScanner(QObject *parent = nullptr);

    void requestScan(const QString &projectRootPath);
    void setDebounceIntervalMs(int intervalMs);

signals:
    void scanFinished(const TherionStudio::ProjectOutputsScanner::Result &result);

private slots:
    void startScan();
    void handleScanFinished();

private:
    QString pendingProjectRootPath_;
    bool hasPendingRequest_ = false;
    bool queuedScan_ = false;
    quint64 generation_ = 0;
    QTimer *debounceTimer_ = nullptr;
    QFutureWatcher<Result> *scanWatcher_ = nullptr;
};

} // namespace TherionStudio
