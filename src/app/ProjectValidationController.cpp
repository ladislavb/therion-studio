#include "ProjectValidationController.h"

namespace TherionStudio
{
ProjectValidationController::ProjectValidationController(QObject *parent)
    : ProjectValidationController(std::make_shared<ProjectScanCacheService>(), parent)
{
}

ProjectValidationController::ProjectValidationController(std::shared_ptr<ProjectScanCacheService> scanCacheService,
                                                         QObject *parent)
    : QObject(parent)
    , scanner_(new ProjectValidationScanner(scanCacheService != nullptr
                                                ? std::move(scanCacheService)
                                                : std::make_shared<ProjectScanCacheService>(),
                                            this))
{
    connect(scanner_,
            &ProjectValidationScanner::validationStarted,
            this,
            &ProjectValidationController::handleScannerStarted);
    connect(scanner_,
            &ProjectValidationScanner::validationFinished,
            this,
            &ProjectValidationController::handleScannerFinished);
}

void ProjectValidationController::requestValidation(const Request &request)
{
    pendingTrigger_ = request.trigger;
    pendingRequestSerial_ = ++latestRequestedSerial_;
    scanner_->requestScan(request.projectRootPath,
                          request.preferredConfigPath,
                          request.validationCatalog,
                          request.inMemoryProjectContentsByPath);
}

void ProjectValidationController::setDebounceIntervalMs(int intervalMs)
{
    scanner_->setDebounceIntervalMs(intervalMs);
}

void ProjectValidationController::handleScannerStarted(quint64 generation, const QString &projectRootPath)
{
    triggersByGeneration_.insert(generation, pendingTrigger_);
    requestSerialByGeneration_.insert(generation, pendingRequestSerial_);
    emit validationStarted(pendingTrigger_, generation, projectRootPath);
}

void ProjectValidationController::handleScannerFinished(const ProjectValidationScanner::Result &result)
{
    const Trigger trigger = triggersByGeneration_.take(result.generation);
    const quint64 requestSerial = requestSerialByGeneration_.take(result.generation);
    if (requestSerial < latestRequestedSerial_) {
        return;
    }
    emit validationFinished(trigger, result);
}
}
