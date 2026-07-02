#include "MapEditorTab.h"

#include "MapEditorLineExtensionPlanner.h"
#include "MapEditorSourceReferenceResolver.h"
#include "../TextEditorSourceTransactionController.h"
#include "../TextEditorTab.h"
#include "../../../core/TherionDocumentEditor.h"

#include <QTimer>

namespace TherionStudio
{
namespace
{
int lineVertexIndexForSourceVertex(const MapGeometryFeature &lineFeature, int sourceVertexIndex)
{
    if (sourceVertexIndex < 0) {
        return -1;
    }

    for (int index = 0; index < lineFeature.lineVertices.size(); ++index) {
        if (lineFeature.lineVertices.at(index).anchorSourceVertexIndex == sourceVertexIndex) {
            return index;
        }
    }

    return -1;
}

bool applyLineCoordinateRowsRewriteEdits(const QString &beforeText,
                                         int lineNumber,
                                         const QStringList &coordinateRows,
                                         QString *afterText,
                                         QString *errorMessage)
{
    if (afterText == nullptr) {
        return false;
    }

    QVector<TherionSourceTextEdit> edits;
    if (!TherionDocumentEditor::lineCoordinateRowsRewriteEdits(beforeText, lineNumber, coordinateRows, &edits, errorMessage)) {
        return false;
    }

    QString updatedText = beforeText;
    if (!TherionDocumentEditor::applySourceTextEdits(&updatedText, edits, errorMessage)) {
        return false;
    }

    *afterText = updatedText;
    return true;
}

}

bool MapEditorTab::beginLineExtensionFromSelection(int lineNumber, int sourceVertexIndex, bool prepend)
{
    if (textEditor_ == nullptr || lineNumber <= 0 || sourceVertexIndex < 0) {
        return false;
    }

    const std::optional<MapGeometryFeature> lineFeature = lineFeatureForLineNumber(logicalCommandsForCurrentDocument(),
                                                                                   lineNumber);
    if (!lineFeature.has_value() || lineFeature->lineVertices.size() < 2) {
        return false;
    }

    const int vertexIndex = lineVertexIndexForSourceVertex(lineFeature.value(), sourceVertexIndex);
    if ((prepend && vertexIndex != 0) || (!prepend && vertexIndex != lineFeature->lineVertices.size() - 1)) {
        return false;
    }

    clearInteractiveDrawSession(false);
    interactiveDrawState_.lineExtensionActive_ = true;
    interactiveDrawState_.lineExtensionLineNumber_ = lineNumber;
    interactiveDrawState_.lineExtensionPrepend_ = prepend;
    interactiveDrawState_.mode_ = InteractiveDrawMode::Line;
    selectModeActive_ = false;

    const MapGeometryFeature::TH2LineVertex &endpoint = lineFeature->lineVertices.at(vertexIndex);
    MapEditorInteractiveLineDraftVertex seed;
    seed.anchorSource = endpoint.anchor;
    seed.anchorScene = scenePointFromSourcePosition(endpoint.anchor);
    interactiveDrawState_.lineVertices_.append(seed);
    updateInteractiveDrawPreview();
    emit modeStatusChanged();
    updateCommandSurfaceState();

    toolbarStatusNote_ = prepend
        ? tr("Extend Before: continue drawing from the first vertex, then press Enter or Complete Draft.")
        : tr("Extend After: continue drawing from the last vertex, then press Enter or Complete Draft.");
    refreshToolbarSummary();
    return true;
}

bool MapEditorTab::commitLineExtensionSession()
{
    if (!interactiveDrawState_.lineExtensionActive_) {
        return false;
    }
    if (textEditor_ == nullptr) {
        toolbarStatusNote_ = tr("Extend line failed: no active TH2 text editor.");
        refreshToolbarSummary();
        return true;
    }
    const QString beforeText = textEditor_->text();
    const std::optional<MapGeometryFeature> lineFeature = lineFeatureForLineNumber(beforeText, interactiveDrawState_.lineExtensionLineNumber_);
    if (!lineFeature.has_value()) {
        toolbarStatusNote_ = tr("Extend line failed: line geometry could not be resolved.");
        refreshToolbarSummary();
        return true;
    }

    const MapEditorLineExtensionPlan extensionPlan =
        MapEditorLineExtensionPlanner::planEdit(lineFeature.value(),
                                                interactiveDrawState_.lineVertices_,
                                                interactiveDrawState_.lineExtensionPrepend_);
    if (!extensionPlan.resolved) {
        toolbarStatusNote_ = extensionPlan.errorMessage.isEmpty()
            ? tr("Extend line failed.")
            : extensionPlan.errorMessage;
        refreshToolbarSummary();
        return true;
    }

    const QStringList coordinateRows = coordinateRowsForLineVertices(extensionPlan.editedVertices,
                                                                     lineFeature->closed);
    QString afterText;
    QString errorMessage;
    if (!applyLineCoordinateRowsRewriteEdits(beforeText,
                                             interactiveDrawState_.lineExtensionLineNumber_,
                                             coordinateRows,
                                             &afterText,
                                             &errorMessage)) {
        toolbarStatusNote_ = errorMessage.isEmpty()
            ? tr("Extend line failed.")
            : tr("Extend line failed: %1").arg(errorMessage);
        refreshToolbarSummary();
        return true;
    }

    const int extendedLineNumber = interactiveDrawState_.lineExtensionLineNumber_;
    const int restoredVertexIndex = extensionPlan.restoredVertexIndex;
    const std::optional<MapGeometryFeature> refreshedFeature = lineFeatureForLineNumber(afterText, extendedLineNumber);
    const int restoredSourceVertexIndex =
        refreshedFeature.has_value() && restoredVertexIndex >= 0 && restoredVertexIndex < refreshedFeature->lineVertices.size()
        ? refreshedFeature->lineVertices.at(restoredVertexIndex).anchorSourceVertexIndex
        : -1;
    const TextEditorSourceTransactionResult transactionResult = applySourceTextChangeWithSnapshot(
        tr("Extend Line"),
        beforeText,
        afterText,
        extendedLineNumber,
        std::function<void()>([this, extendedLineNumber, restoredSourceVertexIndex]() {
            interactiveDrawState_.lineExtensionActive_ = false;
            interactiveDrawState_.lineExtensionLineNumber_ = 0;
            interactiveDrawState_.lineExtensionPrepend_ = false;
            clearInteractiveDrawSession(true);
            if (restoredSourceVertexIndex >= 0) {
                restoreLineAnchorSelection(extendedLineNumber, restoredSourceVertexIndex);
                QTimer::singleShot(0, this, [this, extendedLineNumber, restoredSourceVertexIndex]() {
                    restoreLineAnchorSelection(extendedLineNumber, restoredSourceVertexIndex);
                });
            }
            toolbarStatusNote_ = tr("Extended line on source line %1.").arg(extendedLineNumber);
            refreshToolbarSummary();
            updateHelpPanel();
            updateCommandSurfaceState();
        }));
    if (transactionResult != TextEditorSourceTransactionResult::Applied) {
        refreshToolbarSummary();
        updateCommandSurfaceState();
    }
    return true;
}

}
