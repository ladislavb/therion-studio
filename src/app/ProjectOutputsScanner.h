#pragma once

#include <QObject>
#include <QString>
#include <QVector>

#include <functional>

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
        quint64 requestSerial = 0;
        QString projectRootPath;
        QString errorMessage;
        QVector<Artifact> artifacts;
    };

    using ScanFunction = std::function<Result(const QString &projectRootPath, quint64 requestSerial)>;

    explicit ProjectOutputsScanner(QObject *parent = nullptr);
    explicit ProjectOutputsScanner(ScanFunction scanFunction, QObject *parent = nullptr);

    void requestScan(const QString &projectRootPath);
    void setDebounceIntervalMs(int intervalMs);
    bool isLatestRequestResult(const Result &result) const;

signals:
    void scanFinished(const TherionStudio::ProjectOutputsScanner::Result &result);

private slots:
    void startScan();
    void handleScanFinished();

private:
    struct Request
    {
        quint64 requestSerial = 0;
        QString projectRootPath;
    };

    Request pendingRequest_;
    bool hasPendingRequest_ = false;
    bool queuedScan_ = false;
    quint64 latestRequestSerial_ = 0;
    QTimer *debounceTimer_ = nullptr;
    QFutureWatcher<Result> *scanWatcher_ = nullptr;
    ScanFunction scanFunction_;
};

} // namespace TherionStudio
