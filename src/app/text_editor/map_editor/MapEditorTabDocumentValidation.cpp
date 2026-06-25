#include "MapEditorTab.h"

#include "../TextEditorTab.h"

namespace TherionStudio
{
TherionSourceValidationResult MapEditorTab::validateDocument() const
{
    if (textEditor_ != nullptr) {
        return textEditor_->validateDocument();
    }
    return {};
}

void MapEditorTab::setProjectValidationDiagnostics(const QVector<TherionSourceDiagnostic> &diagnostics)
{
    projectValidationDiagnostics_ = diagnostics;
    if (textEditor_ == nullptr) {
        return;
    }

    textEditor_->setProjectValidationDiagnostics(workspaceMode_ == WorkspaceMode::Raw
                                                     ? projectValidationDiagnostics_
                                                     : QVector<TherionSourceDiagnostic>{});
}

bool MapEditorTab::applyValidationFixes(const QVector<TherionSourceDiagnosticFix> &fixes)
{
    if (textEditor_ == nullptr) {
        return false;
    }
    const bool applied = textEditor_->applyValidationFixes(fixes);
    if (applied) {
        refreshMapScenePreservingUndoStack();
    }
    return applied;
}
}
