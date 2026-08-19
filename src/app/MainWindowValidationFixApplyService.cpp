#include "MainWindowValidationFixApplyService.h"

namespace TherionStudio
{
bool MainWindowValidationFixApplyService::applyValidationFixes(const QString &filePath,
                                                               const QString &validationDocumentPath,
                                                               const QVector<TherionSourceDiagnosticFix> &fixes,
                                                               const MainWindowValidationFixApplyContext &context)
{
    if (fixes.isEmpty()) {
        return false;
    }
    for (const TherionSourceDiagnosticFix &fix : fixes) {
        if (fix.expectedSourceDigest.isEmpty()) {
            return false;
        }
    }

    const QString targetPath = filePath.isEmpty() ? validationDocumentPath : filePath;
    if (!targetPath.isEmpty()) {
        const auto openPlan = MainWindowDocumentOpenController::planOpenProjectSearchResult(targetPath);
        if (openPlan.action == MainWindowDocumentOpenController::OpenProjectSearchResultAction::OpenMapDocument) {
            return context.applyFixesToMapPath != nullptr
                && context.applyFixesToMapPath(targetPath, fixes);
        }

        return context.applyFixesToTextPath != nullptr
            && context.applyFixesToTextPath(targetPath, fixes);
    }

    if (context.applyFixesToCurrentMap != nullptr && context.applyFixesToCurrentMap(fixes)) {
        return true;
    }
    return context.applyFixesToCurrentText != nullptr && context.applyFixesToCurrentText(fixes);
}
}
