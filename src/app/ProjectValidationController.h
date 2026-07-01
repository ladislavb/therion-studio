#pragma once

#include "ProjectValidationScanner.h"

#include <QObject>
#include <QHash>
#include <QString>

#include <memory>

namespace TherionStudio
{
class ProjectValidationController final : public QObject
{
    Q_OBJECT

public:
    enum class Trigger
    {
        ManualRefresh,
        ProjectOpened,
        DocumentSaved,
        DocumentChanged,
        ProjectFilesChanged,
        FixApplied
    };

    struct Request
    {
        Trigger trigger = Trigger::ManualRefresh;
        QString projectRootPath;
        TherionSourceValidationCatalog validationCatalog;
        QHash<QString, QString> inMemoryProjectContentsByPath;
    };

    explicit ProjectValidationController(QObject *parent = nullptr);
    explicit ProjectValidationController(std::shared_ptr<ProjectScanCacheService> scanCacheService,
                                         QObject *parent = nullptr);

    void requestValidation(const Request &request);
    void setDebounceIntervalMs(int intervalMs);

signals:
    void validationStarted(TherionStudio::ProjectValidationController::Trigger trigger,
                           quint64 generation,
                           const QString &projectRootPath);
    void validationFinished(TherionStudio::ProjectValidationController::Trigger trigger,
                            const TherionStudio::ProjectValidationScanner::Result &result);

private:
    void handleScannerStarted(quint64 generation, const QString &projectRootPath);
    void handleScannerFinished(const TherionStudio::ProjectValidationScanner::Result &result);

    ProjectValidationScanner *scanner_ = nullptr;
    Trigger pendingTrigger_ = Trigger::ManualRefresh;
    QHash<quint64, Trigger> triggersByGeneration_;
};
}
