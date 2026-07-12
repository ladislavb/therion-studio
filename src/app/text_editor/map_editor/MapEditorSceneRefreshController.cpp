#include "MapEditorSceneRefreshController.h"

#include "MapEditorObjectDetailsLogic.h"
#include "MapEditorSceneInternals.h"
#include "MapEditorSceneSupport.h"
#include "MapEditorSourceReferenceResolver.h"
#include "../../../core/TherionSourceDocument.h"
#include "../../../platform/DiagnosticLogging.h"

#include <QGraphicsScene>
#include <QGraphicsView>
#include <QDebug>
#include <QElapsedTimer>
#include <QObject>
#include <QScrollBar>
#include <QTransform>
#include <QUndoStack>

#include <optional>
#include <utility>

namespace TherionStudio
{
namespace
{
QString mapVertexItemKey(int lineNumber, int vertexIndex, const QString &geometryKind)
{
    return QStringLiteral("%1:%2:%3")
        .arg(lineNumber)
        .arg(vertexIndex)
        .arg(geometryKind.trimmed().toLower());
}

MapEditableGeometryVertexItem *indexedGeometryVertexItem(const MapEditorSceneRefreshContext &context,
                                                         int lineNumber,
                                                         int vertexIndex,
                                                         const QString &geometryKind)
{
    if (context.vertexItemsByKey == nullptr || lineNumber <= 0 || vertexIndex < 0) {
        return nullptr;
    }

    auto itemIt = context.vertexItemsByKey->constFind(mapVertexItemKey(lineNumber, vertexIndex, geometryKind));
    if (itemIt == context.vertexItemsByKey->constEnd()) {
        return nullptr;
    }
    return dynamic_cast<MapEditableGeometryVertexItem *>(itemIt.value());
}

bool diagnosticMapInputLoggingEnabled()
{
    return TherionStudio::diagnosticLoggingEnabled();
}
}

MapEditorSceneRefreshController::MapEditorSceneRefreshController(MapEditorSceneRefreshContext context)
    : context_(std::move(context))
{
}

QGraphicsScene *MapEditorSceneRefreshController::scene() const
{
    return context_.scene != nullptr ? *context_.scene : nullptr;
}

void MapEditorSceneRefreshController::updateSceneRectForBackgroundBounds()
{
    QGraphicsScene *mapScene = scene();
    if (mapScene == nullptr) {
        return;
    }

    const QRectF backgroundBounds = context_.mapBackgroundFitBounds
        ? context_.mapBackgroundFitBounds()
        : QRectF();
    mapScene->setSceneRect(mapEditorScrollableSceneRect(backgroundBounds));
}

bool restoreSceneRefreshSelection(const MapEditorSceneRefreshContext &context)
{
    const int lineNumber = context.sceneRefreshSelectionLineNumber
        ? context.sceneRefreshSelectionLineNumber()
        : 0;
    if (lineNumber <= 0) {
        return false;
    }

    const QString kind = context.sceneRefreshSelectionKind
        ? context.sceneRefreshSelectionKind().trimmed().toLower()
        : QString();
    const int vertexIndex = context.sceneRefreshSelectionVertexIndex
        ? context.sceneRefreshSelectionVertexIndex()
        : -1;
    if (vertexIndex >= 0 && !kind.isEmpty() && context.scene != nullptr && *context.scene != nullptr) {
        if (MapEditableGeometryVertexItem *vertexItem = indexedGeometryVertexItem(context, lineNumber, vertexIndex, kind)) {
            (*context.scene)->clearSelection();
            vertexItem->setVisible(true);
            vertexItem->setSelected(true);
            return true;
        }
        const auto items = (*context.scene)->items();
        for (QGraphicsItem *item : items) {
            auto *vertexItem = dynamic_cast<MapEditableGeometryVertexItem *>(item);
            if (vertexItem == nullptr) {
                continue;
            }
            if (vertexItem->lineNumber() != lineNumber || vertexItem->vertexIndex() != vertexIndex) {
                continue;
            }
            const QString vertexKind = vertexItem->geometryKind().trimmed().toLower();
            const bool kindMatches = kind == QStringLiteral("line control")
                ? vertexKind.startsWith(kind)
                : vertexKind == kind;
            if (!kindMatches) {
                continue;
            }
            (*context.scene)->clearSelection();
            vertexItem->setVisible(true);
            vertexItem->setSelected(true);
            return true;
        }
    }
    if (kind == QStringLiteral("point") && context.restorePointSelection) {
        context.restorePointSelection(lineNumber);
        return true;
    }
    if (kind == QStringLiteral("line") && vertexIndex >= 0 && context.restoreLineAnchorSelection) {
        context.restoreLineAnchorSelection(lineNumber, vertexIndex);
        return true;
    }

    context.selectMapLine(lineNumber, false);
    return true;
}

int fallbackSceneRefreshSelectionLine(const MapEditorSceneRefreshContext &context,
                                      const QVector<TherionSourceLogicalCommand> &commands,
                                      const QVector<TherionParsedLine> &parsedLines)
{
    const int cursorLine = context.currentLineNumber ? context.currentLineNumber() : 0;
    const int cursorColumn = context.currentColumnNumber ? context.currentColumnNumber() : 1;
    if (cursorLine <= 0) {
        return 0;
    }

    const CursorGeometrySelection cursorSelection = !commands.isEmpty()
        ? cursorGeometrySelectionForTextCursor(commands, cursorLine, cursorColumn)
        : cursorGeometrySelectionForTextCursor(parsedLines, cursorLine, cursorColumn);
    return cursorSelection.featureLineNumber > 0
        ? cursorSelection.featureLineNumber
        : cursorLine;
}

const MapEditorOrientationApplicabilityByCommand &MapEditorSceneRefreshController::orientationApplicabilityByCommand() const
{
    Q_ASSERT(context_.orientationApplicabilityByCommand != nullptr);
    static const MapEditorOrientationApplicabilityByCommand emptyApplicability;
    return context_.orientationApplicabilityByCommand != nullptr
        ? *context_.orientationApplicabilityByCommand
        : emptyApplicability;
}

void MapEditorSceneRefreshController::buildMapScene()
{
    *context_.scene = new QGraphicsScene(context_.sceneParent);
    context_.view->setScene(*context_.scene);
    QObject::connect(*context_.scene,
                     &QGraphicsScene::selectionChanged,
                     context_.selectionConnectionContext,
                     [onSelectionChanged = context_.handleSceneSelectionChanged]() {
                         onSelectionChanged();
                     });
}

void MapEditorSceneRefreshController::refreshMapScene()
{
    if (scene() == nullptr) {
        return;
    }

    if (*context_.commandApplyInProgress) {
        *context_.sceneRefreshPending = true;
        return;
    }

    if (context_.undoStack != nullptr) {
        context_.undoStack->clear();
    }

    refreshMapScenePreservingUndoStack();
}

void MapEditorSceneRefreshController::refreshMapScenePreservingUndoStack()
{
    refreshMapScenePreservingUndoStack(false);
}

void MapEditorSceneRefreshController::refreshMapScenePreservingUndoStack(bool preserveViewport)
{
    QGraphicsScene *mapScene = scene();
    if (mapScene == nullptr) {
        return;
    }
    const bool logTiming = diagnosticMapInputLoggingEnabled();
    QElapsedTimer totalTimer;
    QElapsedTimer stageTimer;
    if (logTiming) {
        totalTimer.start();
        stageTimer.start();
    }
    const int beforeItemCount = logTiming ? mapScene->items().size() : -1;
    if (context_.lineVertexSelectionRestoreGeneration != nullptr) {
        ++(*context_.lineVertexSelectionRestoreGeneration);
    }
    const bool canPreserveViewport = preserveViewport
        && context_.view != nullptr
        && context_.view->viewport() != nullptr;
    const QTransform preservedTransform = canPreserveViewport
        ? context_.view->transform()
        : QTransform();
    const QRectF preservedSceneRect = canPreserveViewport ? mapScene->sceneRect() : QRectF();
    const QPointF preservedCenter = canPreserveViewport
        ? context_.view->mapToScene(context_.view->viewport()->rect().center())
        : QPointF();
    QScrollBar *preservedHorizontalScrollBar = canPreserveViewport
        ? context_.view->horizontalScrollBar()
        : nullptr;
    QScrollBar *preservedVerticalScrollBar = canPreserveViewport
        ? context_.view->verticalScrollBar()
        : nullptr;
    const int preservedHorizontalScrollValue = preservedHorizontalScrollBar != nullptr
        ? preservedHorizontalScrollBar->value()
        : 0;
    const int preservedVerticalScrollValue = preservedVerticalScrollBar != nullptr
        ? preservedVerticalScrollBar->value()
        : 0;
    const qint64 preserveMs = logTiming ? stageTimer.restart() : 0;

    if (context_.undoStack != nullptr) {
        context_.updateCommandSurfaceState();
    }
    const qint64 commandSurfaceBeforeMs = logTiming ? stageTimer.restart() : 0;

    context_.clearMapScene();
    const qint64 clearMs = logTiming ? stageTimer.restart() : 0;

    const QVector<TherionSourceLogicalCommand> logicalCommands =
        context_.logicalSource.logicalCommandsForCurrentDocument
            ? context_.logicalSource.logicalCommandsForCurrentDocument()
            : QVector<TherionSourceLogicalCommand>();
    const bool hasLogicalSource = context_.logicalSource.logicalCommandsForCurrentDocument != nullptr;
    std::optional<QVector<TherionParsedLine>> parsedLines;
    auto parsedLinesForRefresh = [&]() -> const QVector<TherionParsedLine> & {
        if (!parsedLines.has_value()) {
            parsedLines = context_.parsedLinesForCurrentDocument
                ? context_.parsedLinesForCurrentDocument()
                : TherionSourceDocument::fromText(context_.documentText()).tokenLines();
        }
        return parsedLines.value();
    };
    const qint64 parseMs = logTiming ? stageTimer.restart() : 0;
    const QVector<MapSceneEntry> entries = hasLogicalSource
        ? collectMapSceneEntries(logicalCommands)
        : collectMapSceneEntries(parsedLinesForRefresh());
    QVector<MapGeometryFeature> geometryFeatures;
    if (context_.logicalSource.geometryProjectionForCurrentDocument
        && hasLogicalSource) {
        const Th2GeometryProjection geometryProjection =
            context_.logicalSource.geometryProjectionForCurrentDocument();
        geometryFeatures = collectGeometryFeatures(geometryProjection, logicalCommands);
    } else {
        geometryFeatures = collectGeometryFeatures(parsedLinesForRefresh());
    }
    QHash<int, TherionParsedLine> parsedLinesByLineNumber;
    if (hasLogicalSource) {
        for (const TherionSourceLogicalCommand &command : logicalCommands) {
            parsedLinesByLineNumber.insert(command.parsed.lineNumber, command.parsed);
        }
    } else {
        for (const TherionParsedLine &parsedLine : parsedLinesForRefresh()) {
            parsedLinesByLineNumber.insert(parsedLine.lineNumber, parsedLine);
        }
    }
    for (MapGeometryFeature &feature : geometryFeatures) {
        if (feature.kind != MapGeometryFeature::Kind::Point) {
            continue;
        }
        const TherionParsedLine parsedLine = parsedLinesByLineNumber.value(feature.lineNumber);
        if (parsedLine.directive == QStringLiteral("point")
            && isOrientationSupportedForParsedLine(parsedLine, orientationApplicabilityByCommand())) {
            feature.orientationSupported = true;
            feature.orientationDegrees = pointOrientationFromParsedLine(parsedLine);
        }
    }
    const qint64 collectMs = logTiming ? stageTimer.restart() : 0;
    const QRectF sourceBounds = context_.mapSourceBoundsForCurrentDocument();
    const std::optional<QRectF> sourceBoundsOverride = sourceBounds.isValid()
        ? std::optional<QRectF>(sourceBounds)
        : std::nullopt;
    const bool hasVisibleBackgroundBounds = context_.mapBackgroundFitBounds
        && context_.mapBackgroundFitBounds().isValid();
    const bool showEmptyDocumentGuides = !hasVisibleBackgroundBounds
        || !geometryFeatures.isEmpty()
        || !entries.isEmpty();
    renderMapWorkspaceScene(mapScene,
                            context_.filePath(),
                            entries,
                            geometryFeatures,
                            sourceBoundsOverride,
                            showEmptyDocumentGuides,
                            context_.itemsByLine,
                            context_.vertexItemsByKey,
                            context_.recordCardMove,
                            context_.recordCardVisibility,
                            context_.recordPointGeometryMove,
                            context_.recordLineAreaVertexMove,
                            context_.recordPointOrientationHandleChange,
                            context_.recordLinePointLeftHandleChange);
    const qint64 renderMs = logTiming ? stageTimer.restart() : 0;

    context_.restoreBackgroundImageItems();
    context_.reprojectMetadataBackgroundLayersForCurrentDocument();
    context_.restoreDraftGeometryItems();
    if (!canPreserveViewport) {
        updateSceneRectForBackgroundBounds();
    }
    const qint64 backgroundMs = logTiming ? stageTimer.restart() : 0;
    const bool restoredSelection = restoreSceneRefreshSelection(context_);
    if (!restoredSelection) {
        context_.selectMapLine(fallbackSceneRefreshSelectionLine(
                                   context_,
                                   logicalCommands,
                                   hasLogicalSource ? QVector<TherionParsedLine>{} : parsedLinesForRefresh()),
                               !preserveViewport);
    }
    const qint64 selectionMs = logTiming ? stageTimer.restart() : 0;
    context_.applyInspectorObjectVisibility();
    context_.updateGeometrySelectionPresentation();
    const qint64 presentationMs = logTiming ? stageTimer.restart() : 0;
    if (canPreserveViewport) {
        // Retaining the original scene rect means the exact scrollbar values
        // still refer to the same scene coordinates. This avoids both the
        // large remap jump and the high-zoom rounding drift of centerOn().
        mapScene->setSceneRect(preservedSceneRect);
        context_.view->setTransform(preservedTransform);
        if (preservedHorizontalScrollBar != nullptr && preservedVerticalScrollBar != nullptr) {
            preservedHorizontalScrollBar->setValue(qBound(preservedHorizontalScrollBar->minimum(),
                                                         preservedHorizontalScrollValue,
                                                         preservedHorizontalScrollBar->maximum()));
            preservedVerticalScrollBar->setValue(qBound(preservedVerticalScrollBar->minimum(),
                                                       preservedVerticalScrollValue,
                                                       preservedVerticalScrollBar->maximum()));
        } else {
            context_.view->centerOn(preservedCenter);
        }
        *context_.autoFitEnabled = false;
        context_.syncZoomFactorFromView();
    } else if (*context_.autoFitEnabled) {
        context_.fitMapToView(*context_.fitBackgroundRequested);
    } else {
        context_.syncZoomFactorFromView();
    }
    const qint64 viewportMs = logTiming ? stageTimer.restart() : 0;
    context_.updateInteractiveDrawPreview();
    context_.refreshStatus();
    context_.updateCommandSurfaceState();
    context_.updateHelpPanel();
    context_.refreshObjectDetailsPanel();
    const qint64 finalUiMs = logTiming ? stageTimer.restart() : 0;

    if (logTiming) {
        const int afterItemCount = mapScene->items().size();
        qInfo().noquote()
            << QStringLiteral(
                   "map-scene-refresh preserve_viewport=%1 before_items=%2 after_items=%3 parsed_lines=%4 entries=%5 "
                   "geometry=%6 restored_selection=%7 preserve_ms=%8 command_surface_before_ms=%9 clear_ms=%10 "
                   "parse_ms=%11 collect_ms=%12 render_ms=%13 background_ms=%14 selection_ms=%15 presentation_ms=%16 "
                   "viewport_ms=%17 final_ui_ms=%18 total_ms=%19")
                   .arg(preserveViewport ? 1 : 0)
                   .arg(beforeItemCount)
                   .arg(afterItemCount)
                   .arg(parsedLines.has_value() ? parsedLines->size() : 0)
                   .arg(entries.size())
                   .arg(geometryFeatures.size())
                   .arg(restoredSelection ? 1 : 0)
                   .arg(preserveMs)
                   .arg(commandSurfaceBeforeMs)
                   .arg(clearMs)
                   .arg(parseMs)
                   .arg(collectMs)
                   .arg(renderMs)
                   .arg(backgroundMs)
                   .arg(selectionMs)
                   .arg(presentationMs)
                   .arg(viewportMs)
                   .arg(finalUiMs)
                   .arg(totalTimer.elapsed());
    }
}

void MapEditorSceneRefreshController::flushPendingMapSceneRefreshAfterCommand()
{
    if (!*context_.sceneRefreshPending) {
        return;
    }

    *context_.sceneRefreshPending = false;
    refreshMapScenePreservingUndoStack(true);
}

}
