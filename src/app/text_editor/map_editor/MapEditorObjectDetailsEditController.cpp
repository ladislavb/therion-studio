#include "MapEditorObjectDetailsEditController.h"

#include "../CommandOptionsDialog.h"
#include "../TextEditorTab.h"
#include "MapEditorAreaReferenceResolver.h"
#include "MapEditorInspectorData.h"
#include "MapEditorObjectDeletePlanner.h"
#include "MapEditorObjectDetailsLogic.h"
#include "MapEditorSceneSupport.h"
#include "MapEditorSourceReferenceResolver.h"
#include "../../../core/TherionDocumentEditor.h"
#include "../../../core/TherionSourceLogicalDocument.h"
#include "../../../core/TherionSourceDocument.h"
#include "../../../core/TherionSourceText.h"
#include "../../../core/TherionTokenRules.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QLineF>
#include <QPlainTextEdit>
#include <QSignalBlocker>
#include <QTimer>

#include <cmath>
#include <optional>
#include <utility>

namespace TherionStudio
{
namespace
{
constexpr int kApplyObjectOrientationOnNextEventLoopMs = 0;

QString normalizedQuickSubtypeForWrite(const QString &subtype)
{
    const QString trimmedSubtype = subtype.trimmed();
    return TherionTokenRules::tokenStartsOption(trimmedSubtype) ? QString() : trimmedSubtype;
}

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

std::optional<std::pair<int, int>> selectedLineAndSourceVertex(const MapEditorObjectDetailsContext &context)
{
    if (context.selectedObjectLineNumber == nullptr
        || context.selectedObjectVertexIndex == nullptr
        || context.selectedObjectKind == nullptr
        || (*context.selectedObjectKind) != QStringLiteral("line")
        || (*context.selectedObjectLineNumber) <= 0
        || (*context.selectedObjectVertexIndex) < 0) {
        return std::nullopt;
    }

    return std::make_pair(*context.selectedObjectLineNumber, *context.selectedObjectVertexIndex);
}

QStringList trimmedLinePointStandaloneRows(const QString &rawRowsText)
{
    QStringList rows = rawRowsText.split(QLatin1Char('\n'), Qt::KeepEmptyParts);
    QStringList trimmedRows;
    trimmedRows.reserve(rows.size());
    for (QString row : rows) {
        if (row.endsWith(QLatin1Char('\r'))) {
            row.chop(1);
        }
        row = row.trimmed();
        if (!row.isEmpty()) {
            trimmedRows.append(row);
        }
    }
    return trimmedRows;
}

const TherionParsedSourceLine *sourceLineAt(const TherionSourceDocument &sourceDocument, int lineNumber)
{
    if (lineNumber <= 0 || lineNumber > sourceDocument.parsedDocument().lines.size()) {
        return nullptr;
    }
    return &sourceDocument.parsedDocument().lines.at(lineNumber - 1);
}

bool requireSourceTransaction(const MapEditorObjectDetailsContext &context, const QString &message)
{
    if (context.applySourceTextChangeWithSnapshot) {
        return true;
    }

    Q_ASSERT(context.applySourceTextChangeWithSnapshot);
    if (context.toolbarStatusNote != nullptr) {
        *context.toolbarStatusNote = message;
    }
    if (context.refreshToolbarSummary) {
        context.refreshToolbarSummary();
    }
    if (context.refreshObjectDetailsPanel) {
        context.refreshObjectDetailsPanel();
    }
    return false;
}

TextEditorSourceTransactionResult applySourceTransactionWithOptionalHook(const MapEditorObjectDetailsContext &context,
                                                                         const QString &label,
                                                                         const QString &beforeText,
                                                                         const QString &afterText,
                                                                         int targetLineNumber,
                                                                         std::function<void()> postApplyHook = {})
{
    return context.applySourceTextChangeWithSnapshot(label,
                                                     beforeText,
                                                     afterText,
                                                     targetLineNumber,
                                                     std::move(postApplyHook));
}

bool applySourceTextEdits(QString *contents,
                          const QVector<TherionSourceTextEdit> &edits,
                          QString *errorMessage)
{
    return TherionDocumentEditor::applySourceTextEdits(contents, edits, errorMessage);
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
    if (!applySourceTextEdits(&updatedText, edits, errorMessage)) {
        return false;
    }

    *afterText = updatedText;
    return true;
}

void schedulePointSelectionRecovery(const MapEditorObjectDetailsContext &context, int lineNumber)
{
    if (!context.restorePointSelectionLater) {
        return;
    }

    auto attemptRestore = [context, lineNumber]() {
        context.restorePointSelectionLater(lineNumber);
    };
    if (context.callbackContext != nullptr) {
        QTimer::singleShot(0, context.callbackContext, attemptRestore);
    } else {
        attemptRestore();
    }
}

void scheduleLineAnchorSelectionRecovery(const MapEditorObjectDetailsContext &context,
                                         int lineNumber,
                                         int sourceVertexIndex)
{
    if (!context.restoreLineAnchorSelectionLater) {
        return;
    }

    auto attemptRestore = [context, lineNumber, sourceVertexIndex]() {
        context.restoreLineAnchorSelectionLater(lineNumber, sourceVertexIndex);
    };
    if (context.callbackContext != nullptr) {
        QTimer::singleShot(0, context.callbackContext, attemptRestore);
    } else {
        attemptRestore();
    }
}

qreal defaultLinePointOrientationDegrees(const MapGeometryFeature &feature, int vertexIndex)
{
    if (vertexIndex < 0 || vertexIndex >= feature.lineVertices.size()) {
        return 0.0;
    }

    const MapGeometryFeature::TH2LineVertex &vertex = feature.lineVertices.at(vertexIndex);
    QPointF previous = vertex.incomingControl.value_or(vertex.anchor);
    QPointF next = vertex.outgoingControl.value_or(vertex.anchor);

    qreal previousDistance = std::hypot(previous.x() - vertex.anchor.x(), previous.y() - vertex.anchor.y());
    qreal nextDistance = std::hypot(next.x() - vertex.anchor.x(), next.y() - vertex.anchor.y());
    if (previousDistance <= 1e-6 && vertexIndex > 0) {
        previous = feature.lineVertices.at(vertexIndex - 1).anchor;
        previousDistance = std::hypot(previous.x() - vertex.anchor.x(), previous.y() - vertex.anchor.y());
    }
    if (nextDistance <= 1e-6 && vertexIndex + 1 < feature.lineVertices.size()) {
        next = feature.lineVertices.at(vertexIndex + 1).anchor;
        nextDistance = std::hypot(next.x() - vertex.anchor.x(), next.y() - vertex.anchor.y());
    }

    if (previousDistance > 1e-6 && nextDistance > 1e-6) {
        if (previousDistance > nextDistance) {
            const QPointF delta = next - vertex.anchor;
            next = vertex.anchor + (delta * (previousDistance / nextDistance));
        } else {
            const QPointF delta = previous - vertex.anchor;
            previous = vertex.anchor + (delta * (nextDistance / previousDistance));
        }
    } else if (previousDistance <= 1e-6 && nextDistance <= 1e-6) {
        return 0.0;
    }

    const QPointF tangent = next - previous;
    if (std::hypot(tangent.x(), tangent.y()) <= 1e-6) {
        return 0.0;
    }

    constexpr qreal pi = 3.14159265358979323846;
    qreal orientation = 360.0 - (std::atan2(tangent.y(), tangent.x()) * 180.0 / pi);
    if (feature.reversed) {
        orientation += 180.0;
    }
    return normalizeOrientationDegreesForMapDetails(orientation);
}

std::optional<qreal> defaultLinePointOrientationForSourceVertex(const QString &documentText, int lineNumber, int sourceVertexIndex)
{
    const std::optional<MapGeometryFeature> lineFeature = lineFeatureForLineNumber(documentText, lineNumber);
    if (!lineFeature.has_value() || lineFeature->kind != MapGeometryFeature::Kind::Line) {
        return std::nullopt;
    }

    const int vertexIndex = lineVertexIndexForSourceVertex(lineFeature.value(), sourceVertexIndex);
    if (vertexIndex < 0) {
        return std::nullopt;
    }

    return defaultLinePointOrientationDegrees(lineFeature.value(), vertexIndex);
}

void scheduleObjectOrientationApply(const MapEditorObjectDetailsContext &context)
{
    if (context.callbackContext == nullptr) {
        MapEditorObjectDetailsEditController(context).applyObjectOrientationEdits();
        return;
    }

    QTimer::singleShot(kApplyObjectOrientationOnNextEventLoopMs, context.callbackContext, [context]() {
        if (context.updatingUi == nullptr || *context.updatingUi) {
            return;
        }
        MapEditorObjectDetailsEditController(context).applyObjectOrientationEdits();
    });
}
}

MapEditorObjectDetailsEditController::MapEditorObjectDetailsEditController(MapEditorObjectDetailsContext context)
    : context_(std::move(context))
{
}

QString MapEditorObjectDetailsEditController::tr(const char *text) const
{
    return QCoreApplication::translate("TherionStudio::MapEditorObjectDetailsEditController", text);
}

const InspectorSymbolCatalog &MapEditorObjectDetailsEditController::inspectorSymbolCatalog() const
{
    Q_ASSERT(context_.inspectorSymbolCatalog != nullptr);
    static const InspectorSymbolCatalog emptyCatalog;
    return context_.inspectorSymbolCatalog != nullptr ? *context_.inspectorSymbolCatalog : emptyCatalog;
}

const MapEditorOrientationApplicabilityByCommand &MapEditorObjectDetailsEditController::orientationApplicabilityByCommand() const
{
    Q_ASSERT(context_.orientationApplicabilityByCommand != nullptr);
    static const MapEditorOrientationApplicabilityByCommand emptyApplicability;
    return context_.orientationApplicabilityByCommand != nullptr
        ? *context_.orientationApplicabilityByCommand
        : emptyApplicability;
}

void MapEditorObjectDetailsEditController::populateScrapScaleFromSourceBounds()
{
    if (*context_.updatingUi
        || context_.scrapScaleSourceX1Spin == nullptr
        || context_.scrapScaleSourceY1Spin == nullptr
        || context_.scrapScaleSourceX2Spin == nullptr
        || context_.scrapScaleSourceY2Spin == nullptr
        || context_.scrapScaleRealX1Spin == nullptr
        || context_.scrapScaleRealY1Spin == nullptr
        || context_.scrapScaleRealX2Spin == nullptr
        || context_.scrapScaleRealY2Spin == nullptr
        || context_.scrapScaleUnitCombo == nullptr) {
        return;
    }

    context_.scrapScaleSourceX1Spin->setDecimals(0);
    context_.scrapScaleSourceY1Spin->setDecimals(0);
    context_.scrapScaleSourceX2Spin->setDecimals(0);
    context_.scrapScaleSourceY2Spin->setDecimals(0);
    context_.scrapScaleRealX1Spin->setDecimals(4);
    context_.scrapScaleRealY1Spin->setDecimals(4);
    context_.scrapScaleRealX2Spin->setDecimals(4);
    context_.scrapScaleRealY2Spin->setDecimals(4);

    const InspectorScrapScale scale = defaultInspectorScrapScale(context_.mapSourceBoundsForCurrentDocument());
    context_.scrapScaleSourceX1Spin->setValue(scale.sourcePoint1.x());
    context_.scrapScaleSourceY1Spin->setValue(scale.sourcePoint1.y());
    context_.scrapScaleSourceX2Spin->setValue(scale.sourcePoint2.x());
    context_.scrapScaleSourceY2Spin->setValue(scale.sourcePoint2.y());
    context_.scrapScaleRealX1Spin->setValue(scale.realPoint1.x());
    context_.scrapScaleRealY1Spin->setValue(scale.realPoint1.y());
    context_.scrapScaleRealX2Spin->setValue(scale.realPoint2.x());
    context_.scrapScaleRealY2Spin->setValue(scale.realPoint2.y());
    const int unitIndex = context_.scrapScaleUnitCombo->findText(scale.unitToken);
    context_.scrapScaleUnitCombo->setCurrentIndex(unitIndex >= 0 ? unitIndex : 0);
}

void MapEditorObjectDetailsEditController::applyScrapScaleEdits()
{
    if (*context_.updatingUi
        || context_.textEditor == nullptr
        || context_.scrapScaleSourceX1Spin == nullptr
        || context_.scrapScaleSourceY1Spin == nullptr
        || context_.scrapScaleSourceX2Spin == nullptr
        || context_.scrapScaleSourceY2Spin == nullptr
        || context_.scrapScaleRealX1Spin == nullptr
        || context_.scrapScaleRealY1Spin == nullptr
        || context_.scrapScaleRealX2Spin == nullptr
        || context_.scrapScaleRealY2Spin == nullptr
        || context_.scrapScaleUnitCombo == nullptr) {
        return;
    }

    int targetLineNumber = 0;
    if (*context_.selectedObjectLineNumber > 0 && *context_.selectedObjectKind == QStringLiteral("scrap")) {
        targetLineNumber = *context_.selectedObjectLineNumber;
    } else {
        const int cursorLineNumber = context_.textEditor->currentLineNumber();
        const TherionSourceDocument sourceDocument = TherionSourceDocument::fromText(context_.textEditor->text());
        if (const TherionParsedSourceLine *sourceLine = sourceLineAt(sourceDocument, cursorLineNumber)) {
            if (sourceLine->parsed.directive == QStringLiteral("scrap")) {
                targetLineNumber = cursorLineNumber;
            }
        }
    }

    if (targetLineNumber <= 0) {
        *context_.toolbarStatusNote = tr("Select a scrap to edit its scale.");
        context_.refreshToolbarSummary();
        return;
    }

    InspectorScrapScale scale;
    scale.sourcePoint1 = QPointF(context_.scrapScaleSourceX1Spin->value(), context_.scrapScaleSourceY1Spin->value());
    scale.sourcePoint2 = QPointF(context_.scrapScaleSourceX2Spin->value(), context_.scrapScaleSourceY2Spin->value());
    scale.realPoint1 = QPointF(context_.scrapScaleRealX1Spin->value(), context_.scrapScaleRealY1Spin->value());
    scale.realPoint2 = QPointF(context_.scrapScaleRealX2Spin->value(), context_.scrapScaleRealY2Spin->value());
    scale.unitToken = context_.scrapScaleUnitCombo->currentText().trimmed();
    if (scale.unitToken.isEmpty()) {
        scale.unitToken = QStringLiteral("m");
    }

    if (QLineF(scale.sourcePoint1, scale.sourcePoint2).length() <= 1e-6
        || QLineF(scale.realPoint1, scale.realPoint2).length() <= 1e-6) {
        *context_.toolbarStatusNote = tr("Scrap scale requires two distinct picture points and two distinct real points.");
        context_.refreshToolbarSummary();
        return;
    }

    const QString beforeText = context_.textEditor->text();
    QString afterText = beforeText;
    QString errorMessage;
    QVector<TherionSourceTextEdit> sourceEdits;
    if (!TherionDocumentEditor::scrapScaleRewriteEdits(afterText,
                                                       targetLineNumber,
                                                       scrapScaleExpression(scale),
                                                       &sourceEdits,
                                                       &errorMessage)
        || !applySourceTextEdits(&afterText, sourceEdits, &errorMessage)) {
        *context_.toolbarStatusNote = errorMessage.isEmpty()
            ? tr("Failed to update scrap scale.")
            : tr("Failed to update scrap scale: %1").arg(errorMessage);
        context_.refreshToolbarSummary();
        return;
    }

    if (afterText == beforeText) {
        return;
    }

    if (!requireSourceTransaction(context_, tr("Cannot update scrap scale without map source transaction support."))) {
        return;
    }
    const MapEditorObjectDetailsContext context = context_;
    applySourceTransactionWithOptionalHook(context_,
                                           tr("Set Scrap Scale"),
                                           beforeText,
                                           afterText,
                                           targetLineNumber,
                                           [context]() {
                                               *context.toolbarStatusNote =
                                                   QCoreApplication::translate("TherionStudio::MapEditorObjectDetailsEditController",
                                                                               "Updated scrap scale.");
                                               context.refreshToolbarSummary();
                                               context.refreshObjectDetailsPanel();
                                           });
}

void MapEditorObjectDetailsEditController::handleConfigureObjectSettingsTriggered()
{
    if (context_.textEditor == nullptr) {
        return;
    }

    QString beforeText = context_.textEditor->text();
    const TherionSourceDocument sourceDocument = TherionSourceDocument::fromText(beforeText);
    const QVector<TherionParsedSourceLine> &sourceLines = sourceDocument.parsedDocument().lines;

    int targetLineNumber = *context_.selectedObjectLineNumber;
    QString targetKind = context_.selectedObjectKind->trimmed().toLower();
    if (!(targetLineNumber > 0 && isConfigurableMapObjectKind(targetKind))) {
        targetLineNumber = context_.textEditor->currentLineNumber();
        if (const TherionParsedSourceLine *sourceLine = sourceLineAt(sourceDocument, targetLineNumber)) {
            targetKind = objectKindForDirective(sourceLine->parsed.directive);
        }
    }

    if (targetLineNumber <= 0 || !isConfigurableMapObjectKind(targetKind)) {
        *context_.toolbarStatusNote = tr("Object settings are available for scrap, point, line, and area commands.");
        context_.refreshToolbarSummary();
        return;
    }

    if (targetLineNumber > sourceLines.size()) {
        *context_.toolbarStatusNote = tr("Selected map object is out of range.");
        context_.refreshToolbarSummary();
        return;
    }

    const TherionParsedSourceLine *targetSourceLine = sourceLineAt(sourceDocument, targetLineNumber);
    if (targetSourceLine == nullptr) {
        *context_.toolbarStatusNote = tr("Selected map object is out of range.");
        context_.refreshToolbarSummary();
        return;
    }
    const QString sourceLine = targetSourceLine->text;
    const TherionParsedLine &parsedLine = targetSourceLine->parsed;
    const QString parsedKind = objectKindForDirective(parsedLine.directive);
    if (parsedKind != targetKind) {
        *context_.toolbarStatusNote = tr("Selected map object source line no longer matches the selection.");
        context_.refreshToolbarSummary();
        return;
    }

    CommandOptionsDialog dialog(CommandOptionsDialogContext{
        .dialogParent = context_.textEditor,
        .commandMetadata = &context_.textEditor->commandMetadata(),
    });
    std::optional<QString> configuredLine =
        dialog.configureLine(targetKind,
                             sourceLine,
                             targetLineNumber,
                             CommandOptionsIdFieldMode::None,
                             CommandOptionsHelpMode::CommandOnly);
    if (!configuredLine.has_value()) {
        return;
    }

    QString updatedLine = configuredLine.value();
    if (parsedLine.commentStart >= 0) {
        const QString comment = sourceLine.mid(parsedLine.commentStart).trimmed();
        if (!comment.isEmpty()) {
            updatedLine += QStringLiteral(" ") + comment;
        }
    }
    if (updatedLine == sourceLine) {
        return;
    }

    TherionSourceText sourceText = TherionSourceText::fromText(beforeText);
    if (!sourceText.replaceLineRange(targetLineNumber - 1, 1, {updatedLine})) {
        *context_.toolbarStatusNote = tr("Selected map object source line no longer exists.");
        context_.refreshToolbarSummary();
        return;
    }
    const QString afterText = sourceText.toText();
    if (!requireSourceTransaction(context_, tr("Cannot edit object settings without map source transaction support."))) {
        return;
    }
    const MapEditorObjectDetailsContext context = context_;
    applySourceTransactionWithOptionalHook(context_,
                                           tr("Edit All Object Settings"),
                                           beforeText,
                                           afterText,
                                           targetLineNumber,
                                           [context]() {
                                               context.refreshObjectDetailsPanel();
                                           });
}

void MapEditorObjectDetailsEditController::handleLineClosedToggled(bool checked)
{
    if (*context_.updatingUi
        || context_.textEditor == nullptr
        || *context_.selectedObjectLineNumber <= 0
        || *context_.selectedObjectKind != QStringLiteral("line")) {
        return;
    }

    const int targetLineNumber = *context_.selectedObjectLineNumber;
    const QString beforeText = context_.textEditor->text();
    QString afterText = beforeText;
    QString errorMessage;
    QVector<TherionSourceTextEdit> sourceEdits;
    if (!TherionDocumentEditor::lineOptionToggleRewriteEdits(afterText,
                                                             targetLineNumber,
                                                             QStringLiteral("close"),
                                                             checked,
                                                             &sourceEdits,
                                                             &errorMessage)
        || !applySourceTextEdits(&afterText, sourceEdits, &errorMessage)) {
        *context_.toolbarStatusNote = errorMessage.isEmpty()
            ? tr("Failed to update line closed state.")
            : tr("Failed to update line closed state: %1").arg(errorMessage);
        context_.refreshToolbarSummary();
        context_.refreshObjectDetailsPanel();
        return;
    }

    if (afterText == beforeText) {
        return;
    }

    if (!requireSourceTransaction(context_, tr("Cannot update line closed state without map source transaction support."))) {
        return;
    }
    const MapEditorObjectDetailsContext context = context_;
    applySourceTransactionWithOptionalHook(context_,
                                           tr("Edit Line Closed"),
                                           beforeText,
                                           afterText,
                                           targetLineNumber,
                                           [context, targetLineNumber]() {
                                               if (context.selectMapLine) {
                                                   context.selectMapLine(targetLineNumber, false);
                                               }
                                               context.refreshObjectDetailsPanel();
                                           });
}

void MapEditorObjectDetailsEditController::handleLineReversedToggled(bool checked)
{
    if (*context_.updatingUi
        || context_.textEditor == nullptr
        || *context_.selectedObjectLineNumber <= 0
        || *context_.selectedObjectKind != QStringLiteral("line")) {
        return;
    }

    const int targetLineNumber = *context_.selectedObjectLineNumber;
    const QString beforeText = context_.textEditor->text();
    QString afterText = beforeText;
    QString errorMessage;
    QVector<TherionSourceTextEdit> sourceEdits;
    if (!TherionDocumentEditor::lineOptionToggleRewriteEdits(afterText,
                                                             targetLineNumber,
                                                             QStringLiteral("reverse"),
                                                             checked,
                                                             &sourceEdits,
                                                             &errorMessage)
        || !applySourceTextEdits(&afterText, sourceEdits, &errorMessage)) {
        *context_.toolbarStatusNote = errorMessage.isEmpty()
            ? tr("Failed to update line reverse state.")
            : tr("Failed to update line reverse state: %1").arg(errorMessage);
        context_.refreshToolbarSummary();
        context_.refreshObjectDetailsPanel();
        return;
    }

    if (afterText == beforeText) {
        return;
    }

    if (!requireSourceTransaction(context_, tr("Cannot update line reversed state without map source transaction support."))) {
        return;
    }
    const MapEditorObjectDetailsContext context = context_;
    applySourceTransactionWithOptionalHook(context_,
                                           tr("Edit Line Reversed"),
                                           beforeText,
                                           afterText,
                                           targetLineNumber,
                                           [context, targetLineNumber]() {
                                               if (context.selectMapLine) {
                                                   context.selectMapLine(targetLineNumber, false);
                                               }
                                               context.refreshObjectDetailsPanel();
                                           });
}

void MapEditorObjectDetailsEditController::handleObjectClipDisabledToggled(bool checked)
{
    if (*context_.updatingUi
        || context_.textEditor == nullptr
        || *context_.selectedObjectLineNumber <= 0) {
        return;
    }

    const QString selectedObjectKind = context_.selectedObjectKind->trimmed().toLower();
    if (selectedObjectKind != QStringLiteral("line")
        && selectedObjectKind != QStringLiteral("area")
        && selectedObjectKind != QStringLiteral("point")) {
        return;
    }

    const int targetLineNumber = *context_.selectedObjectLineNumber;
    const QString beforeText = context_.textEditor->text();
    QString afterText = beforeText;
    QString errorMessage;
    QVector<TherionSourceTextEdit> sourceEdits;
    if (!TherionDocumentEditor::mapObjectClipDisabledRewriteEdits(afterText,
                                                                  targetLineNumber,
                                                                  checked,
                                                                  &sourceEdits,
                                                                  &errorMessage)
        || !applySourceTextEdits(&afterText, sourceEdits, &errorMessage)) {
        *context_.toolbarStatusNote = errorMessage.isEmpty()
            ? tr("Failed to update object clipping.")
            : tr("Failed to update object clipping: %1").arg(errorMessage);
        context_.refreshToolbarSummary();
        context_.refreshObjectDetailsPanel();
        return;
    }

    if (afterText == beforeText) {
        return;
    }

    if (!requireSourceTransaction(context_, tr("Cannot update object clipping without map source transaction support."))) {
        return;
    }
    std::function<void()> selectionRestoreHook;
    if (selectedObjectKind == QStringLiteral("point") && context_.restorePointSelectionLater) {
        selectionRestoreHook = [context = context_, targetLineNumber]() {
            context.restorePointSelectionLater(targetLineNumber);
        };
    } else if (selectedObjectKind == QStringLiteral("line") && context_.selectMapLine) {
        selectionRestoreHook = [context = context_, targetLineNumber]() {
            context.selectMapLine(targetLineNumber, false);
        };
    }
    applySourceTransactionWithOptionalHook(context_,
                                           tr("Edit Object Clipping"),
                                           beforeText,
                                           afterText,
                                           targetLineNumber,
                                           std::move(selectionRestoreHook));
}

void MapEditorObjectDetailsEditController::handlePointAlignChanged()
{
    if (*context_.updatingUi
        || context_.textEditor == nullptr
        || context_.pointAlignCombo == nullptr
        || *context_.selectedObjectLineNumber <= 0
        || *context_.selectedObjectKind != QStringLiteral("point")) {
        return;
    }

    const int targetLineNumber = *context_.selectedObjectLineNumber;
    const QString beforeText = context_.textEditor->text();
    QString afterText = beforeText;
    QString errorMessage;
    QVector<TherionSourceTextEdit> sourceEdits;
    const QString align = context_.pointAlignCombo->currentData().toString();
    if (!TherionDocumentEditor::pointAlignRewriteEdits(afterText,
                                                       targetLineNumber,
                                                       align,
                                                       &sourceEdits,
                                                       &errorMessage)
        || !applySourceTextEdits(&afterText, sourceEdits, &errorMessage)) {
        *context_.toolbarStatusNote = errorMessage.isEmpty()
            ? tr("Failed to update point align.")
            : tr("Failed to update point align: %1").arg(errorMessage);
        context_.refreshToolbarSummary();
        context_.refreshObjectDetailsPanel();
        return;
    }

    if (afterText == beforeText) {
        return;
    }

    if (!requireSourceTransaction(context_, tr("Cannot update point align without map source transaction support."))) {
        return;
    }
    std::function<void()> selectionRestoreHook;
    if (context_.restorePointSelectionLater) {
        selectionRestoreHook = [context = context_, targetLineNumber]() {
            context.restorePointSelectionLater(targetLineNumber);
        };
    }
    applySourceTransactionWithOptionalHook(context_,
                                           tr("Edit Point Align"),
                                           beforeText,
                                           afterText,
                                           targetLineNumber,
                                           std::move(selectionRestoreHook));
}

void MapEditorObjectDetailsEditController::applyObjectOrientationEdits()
{
    if (*context_.updatingUi || context_.textEditor == nullptr || *context_.selectedObjectLineNumber <= 0) {
        return;
    }
    if (context_.orientationEnabledCheck == nullptr
        || context_.orientationSpin == nullptr
        || context_.linePointLeftSizeEnabledCheck == nullptr
        || context_.linePointLeftSizeSpin == nullptr) {
        return;
    }
    const QString selectedObjectKind = *context_.selectedObjectKind;
    const int selectedLineNumber = *context_.selectedObjectLineNumber;
    const int selectedSourceVertexIndex = *context_.selectedObjectVertexIndex;
    if (selectedObjectKind != QStringLiteral("point") && selectedObjectKind != QStringLiteral("line")) {
        return;
    }

    const QString beforeText = context_.textEditor->text();
    QString afterText = beforeText;

    const bool enabled = context_.orientationEnabledCheck->isChecked();
    qreal orientation = normalizeOrientationDegreesForMapDetails(context_.orientationSpin->value());
    if (enabled && selectedObjectKind == QStringLiteral("line")) {
        const std::optional<qreal> existingOrientation =
            linePointOrientationForSourceVertex(beforeText,
                                                selectedLineNumber,
                                                selectedSourceVertexIndex);
        if (!existingOrientation.has_value()) {
            if (const std::optional<qreal> defaultOrientation =
                    defaultLinePointOrientationForSourceVertex(beforeText,
                                                               selectedLineNumber,
                                                               selectedSourceVertexIndex)) {
                orientation = defaultOrientation.value();
                const QSignalBlocker blocker(context_.orientationSpin);
                context_.orientationSpin->setValue(orientation);
            }
        }
    }
    const bool leftSizeEnabled = context_.linePointLeftSizeEnabledCheck->isVisible()
        && context_.linePointLeftSizeEnabledCheck->isChecked();
    const qreal leftSize = qMax<qreal>(0.1, context_.linePointLeftSizeSpin->value());
    const TherionSourceDocument sourceDocument = TherionSourceDocument::fromText(beforeText);
    const int lineCount = sourceDocument.parsedDocument().lines.size();
    QString errorMessage;
    QVector<TherionSourceTextEdit> sourceEdits;
    bool rewritten = false;
    if (selectedObjectKind == QStringLiteral("point")) {
        const TherionParsedSourceLine *selectedSourceLine = sourceLineAt(sourceDocument, selectedLineNumber);
        if (selectedSourceLine == nullptr || selectedLineNumber > lineCount) {
            return;
        }
        if (!isOrientationSupportedForParsedLine(selectedSourceLine->parsed, orientationApplicabilityByCommand())) {
            *context_.toolbarStatusNote = tr("Orientation is not supported for this point type.");
            context_.refreshToolbarSummary();
            context_.refreshObjectDetailsPanel();
            return;
        }
        rewritten = TherionDocumentEditor::pointOrientationRewriteEdits(afterText,
                                                                        selectedLineNumber,
                                                                        enabled,
                                                                        orientation,
                                                                        &sourceEdits,
                                                                        &errorMessage)
            && applySourceTextEdits(&afterText, sourceEdits, &errorMessage);
    } else {
        if (selectedSourceVertexIndex < 0) {
            return;
        }

        const std::optional<MapGeometryFeature> lineFeature = lineFeatureForLineNumber(beforeText, selectedLineNumber);
        if (!lineFeature.has_value()
            || lineFeature->kind != MapGeometryFeature::Kind::Line
            || lineVertexIndexForSourceVertex(lineFeature.value(), selectedSourceVertexIndex) < 0) {
            *context_.toolbarStatusNote = tr("Orientation editing is available only for selected line anchor vertices.");
            context_.refreshToolbarSummary();
            context_.refreshObjectDetailsPanel();
            return;
        }
        const TherionParsedSourceLine *selectedSourceLine = sourceLineAt(sourceDocument, selectedLineNumber);
        if (selectedSourceLine == nullptr || selectedLineNumber > lineCount) {
            return;
        }
        const bool orientationSupported =
            isOrientationSupportedForParsedLine(selectedSourceLine->parsed, orientationApplicabilityByCommand());
        const bool leftSizeSupported = isLinePointLeftSizeSupportedForParsedLine(selectedSourceLine->parsed);
        if (!orientationSupported && !leftSizeSupported) {
            *context_.toolbarStatusNote = tr("Orientation is not supported for this line type.");
            context_.refreshToolbarSummary();
            context_.refreshObjectDetailsPanel();
            return;
        }

        rewritten = true;
        if (orientationSupported) {
            rewritten = TherionDocumentEditor::linePointOrientationRewriteEdits(afterText,
                                                                                selectedLineNumber,
                                                                                selectedSourceVertexIndex,
                                                                                enabled,
                                                                                orientation,
                                                                                &sourceEdits,
                                                                                &errorMessage)
                && applySourceTextEdits(&afterText, sourceEdits, &errorMessage);
        }
        if (rewritten && leftSizeSupported) {
            rewritten = TherionDocumentEditor::linePointLeftSizeRewriteEdits(afterText,
                                                                             selectedLineNumber,
                                                                             selectedSourceVertexIndex,
                                                                             leftSizeEnabled,
                                                                             leftSize,
                                                                             &sourceEdits,
                                                                             &errorMessage)
                && applySourceTextEdits(&afterText, sourceEdits, &errorMessage);
        }
    }

    if (!rewritten) {
        *context_.toolbarStatusNote = errorMessage.isEmpty()
            ? tr("Failed to update orientation.")
            : tr("Failed to update orientation: %1").arg(errorMessage);
        context_.refreshToolbarSummary();
        context_.refreshObjectDetailsPanel();
        return;
    }

    std::function<void()> selectionRestoreHook;
    if (selectedObjectKind == QStringLiteral("line") && context_.restoreLineAnchorSelectionLater) {
        selectionRestoreHook = [context = context_, selectedLineNumber, selectedSourceVertexIndex]() {
            context.restoreLineAnchorSelectionLater(selectedLineNumber, selectedSourceVertexIndex);
            scheduleLineAnchorSelectionRecovery(context, selectedLineNumber, selectedSourceVertexIndex);
        };
    } else if (context_.restorePointSelectionLater) {
        selectionRestoreHook = [context = context_, selectedLineNumber]() {
            context.restorePointSelectionLater(selectedLineNumber);
            schedulePointSelectionRecovery(context, selectedLineNumber);
        };
    }

    if (afterText != beforeText) {
        if (!requireSourceTransaction(context_, tr("Cannot update object orientation without map source transaction support."))) {
            return;
        }
        applySourceTransactionWithOptionalHook(context_,
                                               tr("Edit Object Orientation"),
                                               beforeText,
                                               afterText,
                                               selectedLineNumber,
                                               std::move(selectionRestoreHook));
    } else if (selectionRestoreHook) {
        selectionRestoreHook();
    }

    if (selectedObjectKind == QStringLiteral("line")) {
        *context_.toolbarStatusNote = tr("Updated line point options.");
    } else {
        *context_.toolbarStatusNote = enabled
            ? tr("Updated orientation to %1 degrees.").arg(QString::number(orientation, 'f', 3))
            : tr("Cleared orientation override.");
    }

    context_.refreshToolbarSummary();
    context_.refreshObjectDetailsPanel();
}

void MapEditorObjectDetailsEditController::handleObjectOrientationEnabledToggled(bool checked)
{
    if (*context_.updatingUi || context_.orientationSpin == nullptr) {
        return;
    }
    context_.orientationSpin->setEnabled(checked);
    if (context_.pendingInsertLinePointAvailable
        && context_.pendingInsertLinePointAvailable()
        && context_.setPendingInsertLinePointOrientation) {
        context_.setPendingInsertLinePointOrientation(checked, context_.orientationSpin->value());
        context_.refreshObjectDetailsPanel();
        return;
    }
    applyObjectOrientationEdits();
}

void MapEditorObjectDetailsEditController::handleObjectOrientationValueChanged(double value)
{
    if (*context_.updatingUi
        || context_.orientationEnabledCheck == nullptr
        || !context_.orientationEnabledCheck->isChecked()) {
        return;
    }
    if (context_.pendingInsertLinePointAvailable
        && context_.pendingInsertLinePointAvailable()
        && context_.setPendingInsertLinePointOrientation) {
        context_.setPendingInsertLinePointOrientation(true, value);
        return;
    }
    scheduleObjectOrientationApply(context_);
}

void MapEditorObjectDetailsEditController::handleLinePointLeftSizeEnabledToggled(bool checked)
{
    if (*context_.updatingUi || context_.linePointLeftSizeSpin == nullptr) {
        return;
    }
    context_.linePointLeftSizeSpin->setEnabled(checked);
    if (checked && context_.linePointLeftSizeSpin->value() <= 0.0) {
        context_.linePointLeftSizeSpin->setValue(40.0);
    }
    if (context_.pendingInsertLinePointAvailable
        && context_.pendingInsertLinePointAvailable()
        && context_.setPendingInsertLinePointLeftSize) {
        context_.setPendingInsertLinePointLeftSize(checked, context_.linePointLeftSizeSpin->value());
        context_.refreshObjectDetailsPanel();
        return;
    }
    applyObjectOrientationEdits();
}

void MapEditorObjectDetailsEditController::handleLinePointLeftSizeValueChanged(double value)
{
    if (*context_.updatingUi
        || context_.linePointLeftSizeEnabledCheck == nullptr
        || !context_.linePointLeftSizeEnabledCheck->isChecked()) {
        return;
    }
    if (context_.pendingInsertLinePointAvailable
        && context_.pendingInsertLinePointAvailable()
        && context_.setPendingInsertLinePointLeftSize) {
        context_.setPendingInsertLinePointLeftSize(true, value);
        return;
    }
    scheduleObjectOrientationApply(context_);
}

void MapEditorObjectDetailsEditController::deleteSelectedObjectFromSelection()
{
    if (context_.textEditor == nullptr) {
        return;
    }

    int targetLineNumber = *context_.selectedObjectLineNumber;
    if (targetLineNumber <= 0) {
        targetLineNumber = context_.textEditor->currentLineNumber();
    }
    if (targetLineNumber <= 0) {
        return;
    }

    if (!context_.logicalSource.logicalCommandsForCurrentDocument) {
        *context_.toolbarStatusNote = tr("Object deletion failed.");
        context_.refreshToolbarSummary();
        return;
    }

    const QVector<MapEditorAreaReference> areaReferences =
        mapEditorAreaReferencesForBorderLine(context_.logicalSource.logicalCommandsForCurrentDocument(), targetLineNumber);
    if (!areaReferences.isEmpty()) {
        *context_.toolbarStatusNote = tr("This line is used as an area border. Delete the area instead.");
        context_.refreshToolbarSummary();
        return;
    }

    const QString beforeText = context_.textEditor->text();
    const MapEditorObjectDeletePlan deletePlan = planMapEditorObjectDelete(beforeText, targetLineNumber);
    if (!deletePlan.resolved || !deletePlan.changed) {
        *context_.toolbarStatusNote = deletePlan.errorMessage.isEmpty()
            ? tr("Object deletion failed.")
            : tr("Object deletion failed: %1").arg(deletePlan.errorMessage);
        context_.refreshToolbarSummary();
        return;
    }

    if (!requireSourceTransaction(context_, tr("Cannot delete map object without map source transaction support."))) {
        return;
    }
    const MapEditorObjectDetailsContext context = context_;
    applySourceTransactionWithOptionalHook(context_,
                                           tr("Delete Map Object"),
                                           beforeText,
                                           deletePlan.updatedText,
                                           deletePlan.focusLineAfterDelete,
                                           [context, deletePlan]() {
                                               for (int removedLineNumber : deletePlan.removedLineNumbers) {
                                                   context.hiddenInspectorObjectLines->remove(removedLineNumber);
                                               }
                                               *context.lastInspectorClickedObjectLineNumber = 0;
                                               context.clearInspectorObjectSelection();
                                               *context.toolbarStatusNote =
                                                   QCoreApplication::translate("TherionStudio::MapEditorObjectDetailsEditController",
                                                                               "Deleted selected object from source.");
                                               context.refreshToolbarSummary();
                                           });
}

void MapEditorObjectDetailsEditController::applyObjectQuickFieldEdits()
{
    if (*context_.updatingUi
        || context_.textEditor == nullptr
        || context_.quickTypeCombo == nullptr
        || context_.quickSubtypeCombo == nullptr
        || context_.quickIdentifierEdit == nullptr
        || context_.quickNameEdit == nullptr
        || context_.quickTextEdit == nullptr
        || context_.quickValueEdit == nullptr
        || context_.objectQuickCommandKind == nullptr) {
        return;
    }

    if (context_.pendingInsertQuickFields && context_.setPendingInsertQuickFields) {
        std::optional<InspectorObjectQuickFields> pendingFields = context_.pendingInsertQuickFields();
        if (pendingFields.has_value() && !pendingFields->commandKind.trimmed().isEmpty()) {
            pendingFields->type = context_.quickTypeCombo->currentText();
            pendingFields->subtype = normalizedQuickSubtypeForWrite(context_.quickSubtypeCombo->currentText());
            pendingFields->identifier = context_.quickIdentifierEdit->text();
            pendingFields->name = context_.quickNameEdit->text();
            pendingFields->text = context_.quickTextEdit->text();
            pendingFields->value = context_.quickValueEdit->text();
            pendingFields->textVisible = context_.quickTextEdit->isVisible();
            pendingFields->valueVisible = context_.quickValueEdit->isVisible();
            context_.setPendingInsertQuickFields(pendingFields.value());
            context_.refreshObjectDetailsPanel();
            return;
        }
    }

    int targetLineNumber = *context_.selectedObjectLineNumber;
    if (targetLineNumber <= 0) {
        targetLineNumber = context_.textEditor->currentLineNumber();
    }
    if (targetLineNumber <= 0) {
        return;
    }

    const QString beforeText = context_.textEditor->text();
    QString afterText = beforeText;
    QString errorMessage;
    QVector<TherionSourceTextEdit> sourceEdits;
    if (!TherionDocumentEditor::mapObjectQuickFieldsRewriteEdits(afterText,
                                                                 targetLineNumber,
                                                                 context_.quickTypeCombo->currentText(),
                                                                 normalizedQuickSubtypeForWrite(context_.quickSubtypeCombo->currentText()),
                                                                 context_.quickIdentifierEdit->text(),
                                                                 context_.quickNameEdit->text(),
                                                                 context_.quickNameEdit->isVisible(),
                                                                 &sourceEdits,
                                                                 &errorMessage)
        || !applySourceTextEdits(&afterText, sourceEdits, &errorMessage)) {
        *context_.toolbarStatusNote = errorMessage.isEmpty()
            ? tr("Failed to update object fields.")
            : tr("Failed to update object fields: %1").arg(errorMessage);
        context_.refreshToolbarSummary();
        context_.refreshObjectDetailsPanel();
        return;
    }

    const bool currentTypeIsLabel =
        context_.quickTypeCombo->currentText().trimmed().compare(QStringLiteral("label"), Qt::CaseInsensitive) == 0;
    if (context_.quickTextEdit->isVisible()
        && (!TherionDocumentEditor::mapObjectTextOptionRewriteEdits(afterText,
                                                                    targetLineNumber,
                                                                    currentTypeIsLabel ? context_.quickTextEdit->text() : QString(),
                                                                    &sourceEdits,
                                                                    &errorMessage)
            || !applySourceTextEdits(&afterText, sourceEdits, &errorMessage))) {
        *context_.toolbarStatusNote = errorMessage.isEmpty()
            ? tr("Failed to update label text.")
            : tr("Failed to update label text: %1").arg(errorMessage);
        context_.refreshToolbarSummary();
        context_.refreshObjectDetailsPanel();
        return;
    }

    const bool valueSupported =
        inspectorValueOptionSupportedForCommandType(inspectorSymbolCatalog(),
                                                    *context_.objectQuickCommandKind,
                                                    context_.quickTypeCombo->currentText());
    if ((context_.quickValueEdit->isVisible() || valueSupported)
        && (!TherionDocumentEditor::mapObjectValueOptionRewriteEdits(afterText,
                                                                     targetLineNumber,
                                                                     valueSupported ? context_.quickValueEdit->text() : QString(),
                                                                     &sourceEdits,
                                                                     &errorMessage)
            || !applySourceTextEdits(&afterText, sourceEdits, &errorMessage))) {
        *context_.toolbarStatusNote = errorMessage.isEmpty()
            ? tr("Failed to update point value.")
            : tr("Failed to update point value: %1").arg(errorMessage);
        context_.refreshToolbarSummary();
        context_.refreshObjectDetailsPanel();
        return;
    }

    if (afterText == beforeText) {
        return;
    }

    if (!requireSourceTransaction(context_, tr("Cannot update object fields without map source transaction support."))) {
        return;
    }
    const MapEditorObjectDetailsContext context = context_;
    const QString selectedObjectKind = context_.selectedObjectKind != nullptr
        ? context_.selectedObjectKind->trimmed().toLower()
        : QString();
    applySourceTransactionWithOptionalHook(context_,
                                           tr("Edit Object Fields"),
                                           beforeText,
                                           afterText,
                                           targetLineNumber,
                                           [context, selectedObjectKind, targetLineNumber]() {
                                               if (selectedObjectKind == QStringLiteral("point")
                                                   && context.restorePointSelectionLater) {
                                                   context.restorePointSelectionLater(targetLineNumber);
                                               } else if (selectedObjectKind == QStringLiteral("line")
                                                          && context.selectMapLine) {
                                                   context.selectMapLine(targetLineNumber, false);
                                               }
                                               *context.toolbarStatusNote =
                                                   QCoreApplication::translate("TherionStudio::MapEditorObjectDetailsEditController",
                                                                               "Updated object fields.");
                                               context.refreshToolbarSummary();
                                               context.refreshObjectDetailsPanel();
                                           });
}

void MapEditorObjectDetailsEditController::applyScrapProjectionEdit()
{
    if (*context_.updatingUi
        || context_.textEditor == nullptr
        || context_.quickProjectionCombo == nullptr) {
        return;
    }

    if (context_.pendingInsertQuickFields && context_.setPendingInsertQuickFields) {
        std::optional<InspectorObjectQuickFields> pendingFields = context_.pendingInsertQuickFields();
        if (pendingFields.has_value() && pendingFields->commandKind.trimmed().toLower() == QStringLiteral("scrap")) {
            pendingFields->projection = context_.quickProjectionCombo->currentText();
            pendingFields->identifier = context_.quickIdentifierEdit != nullptr ? context_.quickIdentifierEdit->text() : pendingFields->identifier;
            context_.setPendingInsertQuickFields(pendingFields.value());
            context_.refreshObjectDetailsPanel();
            return;
        }
    }

    int targetLineNumber = *context_.selectedObjectLineNumber;
    if (targetLineNumber <= 0) {
        targetLineNumber = context_.textEditor->currentLineNumber();
    }
    if (targetLineNumber <= 0) {
        return;
    }

    const QString beforeText = context_.textEditor->text();
    QString afterText = beforeText;
    QString errorMessage;
    QVector<TherionSourceTextEdit> sourceEdits;
    if (!TherionDocumentEditor::scrapProjectionRewriteEdits(afterText,
                                                            targetLineNumber,
                                                            context_.quickProjectionCombo->currentText(),
                                                            &sourceEdits,
                                                            &errorMessage)
        || !applySourceTextEdits(&afterText, sourceEdits, &errorMessage)) {
        *context_.toolbarStatusNote = errorMessage.isEmpty()
            ? tr("Failed to update scrap projection.")
            : tr("Failed to update scrap projection: %1").arg(errorMessage);
        context_.refreshToolbarSummary();
        context_.refreshObjectDetailsPanel();
        return;
    }

    if (afterText == beforeText) {
        return;
    }

    if (!requireSourceTransaction(context_, tr("Cannot update scrap projection without map source transaction support."))) {
        return;
    }
    const MapEditorObjectDetailsContext context = context_;
    applySourceTransactionWithOptionalHook(context_,
                                           tr("Edit Scrap Projection"),
                                           beforeText,
                                           afterText,
                                           targetLineNumber,
                                           [context]() {
                                               *context.toolbarStatusNote =
                                                   QCoreApplication::translate("TherionStudio::MapEditorObjectDetailsEditController",
                                                                               "Updated scrap projection.");
                                               context.refreshToolbarSummary();
                                               context.refreshObjectDetailsPanel();
                                           });
}

void MapEditorObjectDetailsEditController::updateObjectQuickSubtypeChoices()
{
    if (context_.quickSubtypeCombo == nullptr
        || context_.quickTypeCombo == nullptr
        || context_.objectQuickCommandKind == nullptr) {
        return;
    }

    const QString currentSubtype = context_.quickSubtypeCombo->currentText();
    const InspectorSymbolCatalog &catalog = inspectorSymbolCatalog();
    setEditableComboValues(context_.quickSubtypeCombo,
                           inspectorSubtypeValuesForCommandTypeWithEmptyChoice(catalog,
                                                                               *context_.objectQuickCommandKind,
                                                                               context_.quickTypeCombo->currentText()),
                           currentSubtype);
}

void MapEditorObjectDetailsEditController::insertVertexBeforeFromSelectionPanel()
{
    context_.insertLineVertexFromSelection(true);
    context_.refreshObjectDetailsPanel();
}

void MapEditorObjectDetailsEditController::insertVertexAfterFromSelectionPanel()
{
    context_.insertLineVertexFromSelection(false);
    context_.refreshObjectDetailsPanel();
}

void MapEditorObjectDetailsEditController::splitLineFromSelectionPanel()
{
    context_.splitLineAtSelection();
    context_.refreshObjectDetailsPanel();
}

void MapEditorObjectDetailsEditController::deleteVertexFromSelectionPanel()
{
    context_.removeLineVertexFromSelection();
    context_.refreshObjectDetailsPanel();
}

void MapEditorObjectDetailsEditController::handleLinePointPreviousControlToggled(bool checked)
{
    if (*context_.updatingUi) {
        return;
    }
    if (context_.pendingInsertLinePointAvailable
        && context_.pendingInsertLinePointAvailable()
        && context_.setPendingInsertLinePointControl) {
        context_.setPendingInsertLinePointControl(true, checked);
        context_.refreshObjectDetailsPanel();
        return;
    }
    if (!context_.setLineVertexControlHandleForSelection) {
        return;
    }
    context_.setLineVertexControlHandleForSelection(true, checked);
    context_.refreshObjectDetailsPanel();
}

void MapEditorObjectDetailsEditController::handleLinePointSmoothToggled(bool checked)
{
    if (*context_.updatingUi) {
        return;
    }
    if (context_.pendingInsertLinePointAvailable
        && context_.pendingInsertLinePointAvailable()
        && context_.setPendingInsertLinePointSmooth) {
        context_.setPendingInsertLinePointSmooth(checked);
        context_.refreshObjectDetailsPanel();
        return;
    }
    if (*context_.updatingUi || !context_.setLineVertexSmoothForSelection) {
        return;
    }
    context_.setLineVertexSmoothForSelection(checked);
    context_.refreshObjectDetailsPanel();
}

void MapEditorObjectDetailsEditController::handleLinePointNextControlToggled(bool checked)
{
    if (*context_.updatingUi) {
        return;
    }
    if (context_.pendingInsertLinePointAvailable
        && context_.pendingInsertLinePointAvailable()
        && context_.setPendingInsertLinePointControl) {
        context_.setPendingInsertLinePointControl(false, checked);
        context_.refreshObjectDetailsPanel();
        return;
    }
    if (!context_.setLineVertexControlHandleForSelection) {
        return;
    }
    context_.setLineVertexControlHandleForSelection(false, checked);
    context_.refreshObjectDetailsPanel();
}

bool MapEditorObjectDetailsEditController::applyLinePointStandaloneRowsEdits(const QStringList &updatedRows)
{
    if (context_.textEditor == nullptr
        || context_.toolbarStatusNote == nullptr
        || context_.refreshToolbarSummary == nullptr) {
        return false;
    }
    const std::optional<std::pair<int, int>> lineAndVertex = selectedLineAndSourceVertex(context_);
    if (!lineAndVertex.has_value()) {
        return false;
    }

    const int lineNumber = lineAndVertex->first;
    const int sourceVertexIndex = lineAndVertex->second;
    const QString beforeText = context_.textEditor->text();
    const std::optional<MapGeometryFeature> lineFeature = lineFeatureForLineNumber(beforeText, lineNumber);
    if (!lineFeature.has_value() || lineFeature->kind != MapGeometryFeature::Kind::Line) {
        (*context_.toolbarStatusNote) = tr("Edit line-point options failed: line geometry could not be resolved.");
        context_.refreshToolbarSummary();
        return false;
    }

    const int vertexIndex = lineVertexIndexForSourceVertex(lineFeature.value(), sourceVertexIndex);
    if (vertexIndex < 0 || vertexIndex >= lineFeature->lineVertices.size()) {
        (*context_.toolbarStatusNote) = tr("Edit line-point options failed: selected line point could not be resolved.");
        context_.refreshToolbarSummary();
        return false;
    }

    QVector<MapGeometryFeature::TH2LineVertex> editedVertices = lineFeature->lineVertices;
    if (editedVertices.at(vertexIndex).standaloneOptionRows == updatedRows) {
        return true;
    }
    editedVertices[vertexIndex].standaloneOptionRows = updatedRows;
    const QStringList coordinateRows = coordinateRowsForLineVertices(editedVertices, lineFeature->closed);

    QString afterText;
    QString errorMessage;
    if (!applyLineCoordinateRowsRewriteEdits(beforeText, lineNumber, coordinateRows, &afterText, &errorMessage)) {
        (*context_.toolbarStatusNote) = errorMessage.isEmpty()
            ? tr("Edit line-point options failed.")
            : tr("Edit line-point options failed: %1").arg(errorMessage);
        context_.refreshToolbarSummary();
        return false;
    }
    if (afterText == beforeText) {
        return true;
    }

    if (!requireSourceTransaction(context_, tr("Cannot update line-point options without map source transaction support."))) {
        return false;
    }
    const int restoredVertex = editedVertices.at(vertexIndex).anchorSourceVertexIndex >= 0
        ? editedVertices.at(vertexIndex).anchorSourceVertexIndex
        : sourceVertexIndex;
    const std::function<void()> selectionRestoreHook = context_.restoreLineAnchorSelectionLater
        ? std::function<void()>([context = context_, lineNumber, restoredVertex]() {
              context.restoreLineAnchorSelectionLater(lineNumber, restoredVertex);
              scheduleLineAnchorSelectionRecovery(context, lineNumber, restoredVertex);
          })
        : std::function<void()>();
    const TextEditorSourceTransactionResult transactionResult =
        applySourceTransactionWithOptionalHook(context_,
                                               tr("Edit Line Point Options"),
                                               beforeText,
                                               afterText,
                                               lineNumber,
                                               selectionRestoreHook);
    if (transactionResult != TextEditorSourceTransactionResult::Applied) {
        context_.refreshToolbarSummary();
        if (context_.refreshObjectDetailsPanel) {
            context_.refreshObjectDetailsPanel();
        }
        return false;
    }

    (*context_.toolbarStatusNote) = updatedRows.isEmpty()
        ? tr("Cleared additional line-point options for line %1, point %2.").arg(lineNumber).arg(vertexIndex + 1)
        : tr("Updated additional line-point options for line %1, point %2.").arg(lineNumber).arg(vertexIndex + 1);
    context_.refreshToolbarSummary();
    return true;
}

void MapEditorObjectDetailsEditController::handleLinePointSegmentSubtypeChanged()
{
    if (*context_.updatingUi
        || context_.linePointSegmentSubtypeCombo == nullptr) {
        return;
    }

    if (context_.pendingInsertLinePointAvailable
        && context_.pendingInsertLinePointAvailable()
        && context_.setPendingInsertLinePointSegmentSubtype) {
        context_.setPendingInsertLinePointSegmentSubtype(
            normalizedQuickSubtypeForWrite(context_.linePointSegmentSubtypeCombo->currentText()));
        context_.refreshObjectDetailsPanel();
        return;
    }

    if (context_.linePointFlagsEdit == nullptr) {
        return;
    }

    const QStringList editorRows = trimmedLinePointStandaloneRows(context_.linePointFlagsEdit->toPlainText());
    const bool altitudeManaged = context_.linePointAltitudeAutoCheck != nullptr
        && context_.linePointAltitudeAutoCheck->isVisible();
    const bool altitudeAuto = altitudeManaged && context_.linePointAltitudeAutoCheck->isChecked();
    if (applyLinePointStandaloneRowsEdits(linePointRowsWithStructuredStandaloneOptions(editorRows,
                                                                                       context_.linePointSegmentSubtypeCombo->currentText(),
                                                                                       altitudeAuto,
                                                                                       true,
                                                                                       altitudeManaged))) {
        context_.refreshObjectDetailsPanel();
    }
}

void MapEditorObjectDetailsEditController::handleLinePointAltitudeAutoToggled(bool checked)
{
    if (*context_.updatingUi
        || context_.linePointFlagsEdit == nullptr) {
        return;
    }

    const QStringList editorRows = trimmedLinePointStandaloneRows(context_.linePointFlagsEdit->toPlainText());
    const bool segmentManaged = context_.linePointSegmentSubtypeCombo != nullptr
        && context_.linePointSegmentSubtypeCombo->isVisible();
    const QString segmentSubtype = segmentManaged
        ? context_.linePointSegmentSubtypeCombo->currentText()
        : QString();
    if (applyLinePointStandaloneRowsEdits(linePointRowsWithStructuredStandaloneOptions(editorRows,
                                                                                       segmentSubtype,
                                                                                       checked,
                                                                                       segmentManaged,
                                                                                       true))) {
        context_.refreshObjectDetailsPanel();
    }
}

void MapEditorObjectDetailsEditController::applyLinePointFlagsEdits()
{
    if (context_.linePointFlagsEdit == nullptr) {
        return;
    }

    const QStringList editorRows = trimmedLinePointStandaloneRows(context_.linePointFlagsEdit->toPlainText());
    const bool segmentManaged = context_.linePointSegmentSubtypeCombo != nullptr
        && context_.linePointSegmentSubtypeCombo->isVisible();
    const bool altitudeManaged = context_.linePointAltitudeAutoCheck != nullptr
        && context_.linePointAltitudeAutoCheck->isVisible();
    const QString segmentSubtype = segmentManaged
        ? context_.linePointSegmentSubtypeCombo->currentText()
        : QString();
    const bool altitudeAuto = altitudeManaged && context_.linePointAltitudeAutoCheck->isChecked();
    if (applyLinePointStandaloneRowsEdits(linePointRowsWithStructuredStandaloneOptions(editorRows,
                                                                                       segmentSubtype,
                                                                                       altitudeAuto,
                                                                                       segmentManaged,
                                                                                       altitudeManaged))) {
        context_.refreshObjectDetailsPanel();
    }
}

}
