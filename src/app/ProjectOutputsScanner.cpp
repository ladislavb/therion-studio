#include "ProjectOutputsScanner.h"

#include "ProjectFileDiscovery.h"
#include "../core/TherionFileTypes.h"

#include <QDir>
#include <QFutureWatcher>
#include <QHash>
#include <QTimer>
#include <QtConcurrent>

#include <algorithm>
#include <optional>
#include <utility>

namespace TherionStudio
{
namespace
{
QString artifactDuplicateKey(ProjectOutputsScanner::ArtifactKind kind, const QString &relativePath)
{
    return QString::number(static_cast<int>(kind)) + QLatin1Char('|') + QFileInfo(relativePath).fileName().toCaseFolded();
}

QString relativeFolderForDisplay(const QString &relativePath)
{
    const QString folder = QFileInfo(relativePath).path();
    if (folder.isEmpty()) {
        return QStringLiteral(".");
    }
    return QDir::fromNativeSeparators(folder);
}

void populateArtifactDisplayNames(QVector<ProjectOutputsScanner::Artifact> &artifacts)
{
    QHash<QString, int> nameCounts;
    for (const ProjectOutputsScanner::Artifact &artifact : artifacts) {
        ++nameCounts[artifactDuplicateKey(artifact.kind, artifact.relativePath)];
    }

    for (ProjectOutputsScanner::Artifact &artifact : artifacts) {
        const QString fileName = QFileInfo(artifact.relativePath).fileName();
        if (nameCounts.value(artifactDuplicateKey(artifact.kind, artifact.relativePath)) <= 1) {
            artifact.displayName = fileName;
            continue;
        }
        artifact.displayName = QStringLiteral("%1 (%2)").arg(fileName, relativeFolderForDisplay(artifact.relativePath));
    }
}

std::optional<ProjectOutputsScanner::ArtifactKind> artifactKindForFile(const QFileInfo &info)
{
    if (!info.isFile()) {
        return std::nullopt;
    }

    if (isTherionModelOutputFileName(info.fileName())) {
        return ProjectOutputsScanner::ArtifactKind::Model;
    }
    if (isTherionMapAtlasOutputFileName(info.fileName())) {
        return ProjectOutputsScanner::ArtifactKind::MapAtlas;
    }
    if (isTherionDatabaseOutputFileName(info.fileName())) {
        return ProjectOutputsScanner::ArtifactKind::Database;
    }
    return std::nullopt;
}

ProjectOutputsScanner::Result performProjectOutputsScan(const QString &projectRootPath, quint64 requestSerial)
{
    ProjectOutputsScanner::Result result;
    result.requestSerial = requestSerial;
    if (projectRootPath.trimmed().isEmpty()) {
        result.errorMessage = QObject::tr("Open a project to browse outputs.");
        return result;
    }
    result.projectRootPath = ProjectFileDiscovery::canonicalOrAbsolutePath(projectRootPath);
    if (!QDir(result.projectRootPath).exists()) {
        result.errorMessage = QObject::tr("Open a project to browse outputs.");
        return result;
    }

    const QVector<ProjectDiscoveredFile> outputFiles =
        ProjectFileDiscovery::collectFiles(result.projectRootPath, [](const QFileInfo &info) {
            return artifactKindForFile(info).has_value();
        });
    result.artifacts.reserve(outputFiles.size());
    for (const ProjectDiscoveredFile &file : outputFiles) {
        const std::optional<ProjectOutputsScanner::ArtifactKind> kind = artifactKindForFile(QFileInfo(file.filePath));
        if (!kind.has_value()) {
            continue;
        }
        result.artifacts.append({
            file.filePath,
            file.relativePath,
            QString(),
            *kind,
        });
    }
    std::sort(result.artifacts.begin(),
              result.artifacts.end(),
              [](const ProjectOutputsScanner::Artifact &left, const ProjectOutputsScanner::Artifact &right) {
                  if (left.kind != right.kind) {
                      return static_cast<int>(left.kind) < static_cast<int>(right.kind);
                  }
                  return QString::compare(left.relativePath, right.relativePath, Qt::CaseInsensitive) < 0;
              });
    populateArtifactDisplayNames(result.artifacts);
    return result;
}
}

ProjectOutputsScanner::ProjectOutputsScanner(QObject *parent)
    : ProjectOutputsScanner(performProjectOutputsScan, parent)
{
}

ProjectOutputsScanner::ProjectOutputsScanner(ScanFunction scanFunction, QObject *parent)
    : QObject(parent)
    , debounceTimer_(new QTimer(this))
    , scanWatcher_(new QFutureWatcher<Result>(this))
    , scanFunction_(std::move(scanFunction))
{
    debounceTimer_->setSingleShot(true);
    debounceTimer_->setInterval(120);
    connect(debounceTimer_, &QTimer::timeout, this, &ProjectOutputsScanner::startScan);
    connect(scanWatcher_, &QFutureWatcher<Result>::finished, this, &ProjectOutputsScanner::handleScanFinished);
}

void ProjectOutputsScanner::requestScan(const QString &projectRootPath)
{
    pendingRequest_.requestSerial = ++latestRequestSerial_;
    pendingRequest_.projectRootPath = projectRootPath;
    hasPendingRequest_ = true;
    debounceTimer_->start();
}

void ProjectOutputsScanner::setDebounceIntervalMs(int intervalMs)
{
    debounceTimer_->setInterval(intervalMs);
}

bool ProjectOutputsScanner::isLatestRequestResult(const Result &result) const
{
    return result.requestSerial != 0 && result.requestSerial == latestRequestSerial_;
}

void ProjectOutputsScanner::startScan()
{
    if (!hasPendingRequest_) {
        return;
    }

    if (scanWatcher_->isRunning()) {
        queuedScan_ = true;
        return;
    }

    const Request request = pendingRequest_;
    hasPendingRequest_ = false;
    const ScanFunction scanFunction = scanFunction_;
    auto future = QtConcurrent::run([request, scanFunction]() {
        return scanFunction(request.projectRootPath, request.requestSerial);
    });
    scanWatcher_->setFuture(future);
}

void ProjectOutputsScanner::handleScanFinished()
{
    const Result result = scanWatcher_->result();
    if (isLatestRequestResult(result)) {
        emit scanFinished(result);
    }

    if (queuedScan_) {
        queuedScan_ = false;
        startScan();
    }
}

} // namespace TherionStudio
