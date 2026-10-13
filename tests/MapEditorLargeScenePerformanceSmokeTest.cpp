#include "../src/app/text_editor/map_editor/MapEditorSceneInternals.h"
#include "../src/app/text_editor/map_editor/MapEditorInspectorObjectController.h"
#include "../src/app/text_editor/map_editor/MapEditorInspectorObjectContext.h"
#include "../src/app/text_editor/map_editor/MapEditorSceneGeneration.h"
#include "../src/app/text_editor/map_editor/MapEditorSceneInteractionIndex.h"
#include "../src/app/text_editor/map_editor/MapEditorSelectionController.h"
#include "../src/app/text_editor/map_editor/MapEditorSceneRefreshController.h"
#include "../src/app/text_editor/map_editor/MapEditorSceneSupport.h"
#include "../src/app/text_editor/map_editor/MapEditorObjectStyleCatalog.h"
#include "../src/app/text_editor/map_editor/MapEditorSourceProjectionCache.h"
#include "../src/core/TherionDocumentParser.h"
#include "../src/core/TherionSourceDocument.h"
#include "../src/core/TherionSourceLogicalDocument.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QHash>
#include <QScrollBar>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QString>
#include <QSet>
#include <QUndoStack>
#include <QVector>

#include <algorithm>
#include <iostream>
#include <memory>
#include <optional>

using namespace TherionStudio;

namespace
{
bool expect(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

QString largeBezierMapText(int lineCount, int bezierRowsPerLine)
{
    QString text;
    text.reserve(lineCount * bezierRowsPerLine * 64);
    text += QStringLiteral("encoding utf-8\n\nscrap large-smoke -projection plan\n");
    for (int lineIndex = 0; lineIndex < lineCount; ++lineIndex) {
        const double baseX = static_cast<double>((lineIndex % 25) * 40);
        const double baseY = static_cast<double>((lineIndex / 25) * 25);
        text += QStringLiteral("  line wall -id wall-%1\n").arg(lineIndex + 1);
        text += QStringLiteral("    %1 %2\n").arg(baseX, 0, 'f', 2).arg(baseY, 0, 'f', 2);
        for (int row = 0; row < bezierRowsPerLine; ++row) {
            const double x = baseX + ((row + 1) * 4.0);
            const double y = baseY + ((row % 2 == 0) ? 8.0 : -8.0);
            text += QStringLiteral("    %1 %2 %3 %4 %5 %6\n")
                        .arg(x - 2.0, 0, 'f', 2)
                        .arg(baseY + 6.0, 0, 'f', 2)
                        .arg(x - 1.0, 0, 'f', 2)
                        .arg(y, 0, 'f', 2)
                        .arg(x, 0, 'f', 2)
                        .arg(y, 0, 'f', 2);
        }
        text += QStringLiteral("  endline\n");
    }
    text += QStringLiteral("endscrap\n");
    return text;
}

MapEditableGeometryVertexItem *selectedLineControl(QGraphicsScene *scene)
{
    if (scene == nullptr) {
        return nullptr;
    }
    for (QGraphicsItem *item : scene->selectedItems()) {
        auto *vertexItem = dynamic_cast<MapEditableGeometryVertexItem *>(item);
        if (vertexItem != nullptr && vertexItem->geometryKind() == QStringLiteral("line control")) {
            return vertexItem;
        }
    }
    return nullptr;
}

int runInspectorLineLookupBenchmark()
{
    constexpr int kEntryCount = 2643;
    constexpr int kQueryCount = 300;
    constexpr int kSourceLineRole = Qt::UserRole + 700;
    QStandardItemModel model;
    for (int index = 0; index < kEntryCount / 3; ++index) {
        auto *scrap = new QStandardItem(QStringLiteral("scrap"));
        const int scrapLine = index * 10 + 1;
        scrap->setData(scrapLine, kSourceLineRole);
        for (int child = 1; child <= 2; ++child) {
            auto *object = new QStandardItem(QStringLiteral("object"));
            object->setData(scrapLine + child * 2, kSourceLineRole);
            scrap->appendRow(object);
        }
        model.appendRow(scrap);
    }
    MapEditorInspectorObjectContext context;
    context.objectsModel = &model;
    const MapEditorInspectorObjectController controller(context);

    QElapsedTimer timer;
    timer.start();
    for (int query = 0; query < kQueryCount; ++query) {
        const int group = (query * 137) % (kEntryCount / 3);
        const int expectedLine = group * 10 + 3;
        const QModelIndex result = controller.findInspectorObjectIndexForLine(expectedLine + 1);
        if (!expect(result.isValid() && result.data(kSourceLineRole).toInt() == expectedLine,
                    "Inspector line lookup should return the nearest preceding object.")) {
            return 1;
        }
    }
    std::cout << "inspector-line-lookup entries=" << model.rowCount() * 3
              << " queries=" << kQueryCount
              << " elapsed_ms=" << timer.elapsed() << '\n';
    return 0;
}

int runSelectionPresentationBenchmark(QGraphicsScene *scene,
                                      MapEditorSceneGeneration *sceneGeneration,
                                      const QHash<int, QGraphicsItem *> &itemsByLine)
{
    if (!expect(scene != nullptr && sceneGeneration != nullptr && itemsByLine.size() >= 2,
                "Selection benchmark requires a generated large scene.")) {
        return 1;
    }
    QList<int> lines = itemsByLine.keys();
    std::sort(lines.begin(), lines.end());
    const int firstLine = lines.first();
    const int secondLine = lines.at(1);
    QSet<int> hiddenLines;
    bool updatingSelection = false;
    int selectedLine = 0;
    int selectedVertex = -1;
    int segmentStart = -1;
    int segmentEnd = -1;
    QString selectedKind = QStringLiteral("point");
    MapEditorSceneInteractionIndex interactionIndex;

    const auto selectionContext = [&](bool indexed) {
        MapEditorSelectionContext context;
        context.scene = scene;
        context.sceneGeneration = sceneGeneration;
        context.sceneInteractionIndex = indexed ? &interactionIndex : nullptr;
        context.hiddenObjectLines = &hiddenLines;
        context.updatingSelection = &updatingSelection;
        context.selectedObjectLineNumber = &selectedLine;
        context.selectedObjectVertexIndex = &selectedVertex;
        context.selectedLineSegmentStartVertexIndex = &segmentStart;
        context.selectedLineSegmentEndVertexIndex = &segmentEnd;
        context.selectedObjectKind = &selectedKind;
        return context;
    };
    // Production constructs a fresh controller per call (MapEditorTab::selectionContext()),
    // so any delta state must live in the tab-owned interaction index.
    const auto measure = [&](bool indexed) {
        QElapsedTimer timer;
        timer.start();
        for (int sample = 0; sample < 80; ++sample) {
            scene->clearSelection();
            const int line = sample % 2 == 0 ? firstLine : secondLine;
            if (QGraphicsItem *item = itemsByLine.value(line, nullptr)) {
                item->setSelected(true);
            }
            MapEditorSelectionController(selectionContext(indexed)).updateGeometrySelectionPresentation();
        }
        return timer.elapsed();
    };

    const qint64 baselineMs = measure(false);
    const qint64 indexedMs = measure(true);
    const QGraphicsItem *lastItem = itemsByLine.value(secondLine, nullptr);
    if (!expect(lastItem != nullptr && lastItem->data(kMapSceneInteractionSelectionRole).toBool(),
                "Indexed selection presentation should retain the active line highlight.")) {
        return 1;
    }
    const QGraphicsItem *previousItem = itemsByLine.value(firstLine, nullptr);
    if (!expect(previousItem != nullptr && !previousItem->data(kMapSceneInteractionSelectionRole).toBool(),
                "Indexed selection presentation must clear the previously selected line highlight.")) {
        return 1;
    }
    if (!expect(sceneGeneration->isSettled() && interactionIndex.hasSelectionPresentation(scene, sceneGeneration->current()),
                "Indexed selection benchmark should exercise the settled delta path.")) {
        return 1;
    }
    std::cout << "selection-presentation items=" << scene->items().size()
              << " updates=80 baseline_ms=" << baselineMs
              << " indexed_ms=" << indexedMs << '\n';
    return 0;
}

int runLargeSceneRefreshSmoke()
{
    constexpr int kLineCount = 340;
    constexpr int kBezierRowsPerLine = 3;
    const QString documentText = largeBezierMapText(kLineCount, kBezierRowsPerLine);
    const QVector<TherionParsedLine> parsedLines = TherionDocumentParser::parseTokenLines(documentText);
    const QVector<MapGeometryFeature> geometryFeatures = collectGeometryFeatures(parsedLines);
    if (!expect(geometryFeatures.size() >= kLineCount, "Large map fixture should parse generated line features.")) {
        return 1;
    }

    const MapGeometryFeature &targetLine = geometryFeatures.at(geometryFeatures.size() / 2);
    if (!expect(!targetLine.lineVertices.isEmpty()
                    && targetLine.lineVertices.first().outgoingSourceVertexIndex >= 0,
                "Large map fixture should expose a selectable line control vertex.")) {
        return 1;
    }

    QGraphicsScene *scene = new QGraphicsScene;
    QGraphicsView view;
    view.setScene(scene);
    QUndoStack undoStack;
    QHash<int, QGraphicsItem *> itemsByLine;
    QHash<QString, QGraphicsItem *> vertexItemsByKey;
    bool commandApplyInProgress = false;
    bool sceneRefreshPending = false;
    bool autoFitEnabled = false;
    bool fitBackgroundRequested = false;
    MapEditorSceneGeneration sceneGeneration;
    MapEditorOrientationApplicabilityByCommand orientationApplicability;
    TherionSourceDocumentMetadata metadata;
    metadata.sourceType = TherionSourceDocumentType::TherionMap;
    metadata.revisionId = 1;
    const TherionSourceDocument sourceDocument = TherionSourceDocument::fromText(documentText, metadata);
    const TherionSourceLogicalDocument logicalDocument =
        TherionSourceLogicalDocument::fromSourceDocument(sourceDocument);
    const MapEditorSourceProjectionSnapshotPtr projectionSnapshot =
        std::make_shared<const MapEditorSourceProjectionSnapshot>(MapEditorSourceProjectionSnapshot{
            .revision = metadata.revisionId,
            .logicalCommands = logicalDocument.commands(),
            .geometryProjection = Th2GeometryProjection::fromDocuments(sourceDocument, logicalDocument)});
    int selectedLineNumber = targetLine.lineNumber;
    int selectedVertexIndex = targetLine.lineVertices.first().outgoingSourceVertexIndex;
    QString selectedKind = QStringLiteral("line control");
    int currentLineNumber = targetLine.lineNumber;
    bool restoredSelection = false;
    quint64 completedSceneGeneration = 0;
    std::optional<MapEditorSceneRefreshMetrics> refreshMetrics;

    auto clearScene = [&]() {
        itemsByLine.clear();
        vertexItemsByKey.clear();
        const QList<QGraphicsItem *> items = scene->items();
        for (QGraphicsItem *item : items) {
            scene->removeItem(item);
            delete item;
        }
    };

    const MapEditorObjectStyleCatalog styleCatalog = loadMapEditorObjectStyleCatalog(QString());
    MapEditorSceneRefreshContext context{
        .sceneParent = nullptr,
        .selectionConnectionContext = &view,
        .scene = &scene,
        .view = &view,
        .undoStack = &undoStack,
        .itemsByLine = &itemsByLine,
        .vertexItemsByKey = &vertexItemsByKey,
        .commandApplyInProgress = &commandApplyInProgress,
        .sceneRefreshPending = &sceneRefreshPending,
        .autoFitEnabled = &autoFitEnabled,
        .fitBackgroundRequested = &fitBackgroundRequested,
        .sceneGeneration = &sceneGeneration,
        .orientationApplicabilityByCommand = &orientationApplicability,
        .styleCatalog = &styleCatalog,
        .logicalSource = MapEditorLogicalSourceContext{
            .projectionSnapshotForCurrentDocument = [projectionSnapshot]() {
                return projectionSnapshot;
            },
            .projectionSnapshotWasReused = []() {
                return true;
            }},
        .documentText = [&documentText]() {
            return documentText;
        },
        .parsedLinesForCurrentDocument = [&parsedLines]() {
            return parsedLines;
        },
        .currentLineNumber = [&currentLineNumber]() {
            return currentLineNumber;
        },
        .currentColumnNumber = []() {
            return 1;
        },
        .sceneRefreshSelectionLineNumber = [&selectedLineNumber]() {
            return selectedLineNumber;
        },
        .sceneRefreshSelectionVertexIndex = [&selectedVertexIndex]() {
            return selectedVertexIndex;
        },
        .sceneRefreshSelectionKind = [&selectedKind]() {
            return selectedKind;
        },
        .filePath = []() {
            return QStringLiteral("generated-large-smoke.th2");
        },
        .handleSceneSelectionChanged = []() {},
        .updateCommandSurfaceState = []() {},
        .clearMapScene = clearScene,
        .mapSourceBoundsForCurrentDocument = []() {
            return QRectF();
        },
        .mapBackgroundFitBounds = []() {
            return QRectF();
        },
        .recordCardMove = [](int, const QPointF &, const QPointF &) {},
        .recordCardVisibility = [](int, bool, bool) {},
        .recordPointGeometryMove = [](int, const QPointF &, const QPointF &) {},
        .recordLineAreaVertexMove = [](int, const QString &, int, const QPointF &, const QPointF &) {},
        .recordPointOrientationHandleChange = [](int, qreal) {},
        .recordLinePointLeftHandleChange = [](int, int, qreal, qreal) {},
        .restoreBackgroundImageItems = []() {},
        .reprojectMetadataBackgroundLayersForCurrentDocument = []() {},
        .restoreDraftGeometryItems = []() {},
        .restorePointSelection = [](int) {},
        .restoreLineAnchorSelection = [](int, int) {},
        .selectMapLine = [](int, bool) {},
        .applyInspectorObjectVisibility = []() {},
        .updateGeometrySelectionPresentation = [&restoredSelection]() {
            restoredSelection = true;
        },
        .fitMapToView = [](bool) {},
        .syncZoomFactorFromView = []() {},
        .updateInteractiveDrawPreview = []() {},
        .refreshStatus = []() {},
        .refreshObjectDetailsPanel = []() {},
        .updateHelpPanel = []() {},
        .recordSceneProjectionRefreshCompleted = [&completedSceneGeneration, &sceneGeneration](quint64 generation) {
            completedSceneGeneration = generation;
            sceneGeneration.markCompleted(generation);
        },
        .recordSceneRefreshMetrics = [&refreshMetrics](const MapEditorSceneRefreshMetrics &metrics) {
            refreshMetrics = metrics;
        },
    };

    QElapsedTimer timer;
    timer.start();
    MapEditorSceneRefreshController(context).refreshMapScenePreservingUndoStack();
    const qint64 refreshMs = timer.elapsed();

    const int sceneItems = scene->items().size();
    std::cout << "large-map-scene-refresh"
              << " lines=" << kLineCount
              << " parsed_lines=" << parsedLines.size()
              << " geometry=" << geometryFeatures.size()
              << " scene_items=" << sceneItems
              << " vertex_index=" << vertexItemsByKey.size()
              << " projection_snapshot=" << (refreshMetrics.has_value() && refreshMetrics->usedProjectionSnapshot ? 1 : 0)
              << " projection_cache_reused=" << (refreshMetrics.has_value() && refreshMetrics->projectionSnapshotWasReused ? 1 : 0)
              << " source_fallback=" << (refreshMetrics.has_value() && refreshMetrics->usedSourceFallback ? 1 : 0)
              << " projection_lookup_ms=" << (refreshMetrics.has_value() ? refreshMetrics->projectionLookupMs : -1)
              << " source_fallback_ms=" << (refreshMetrics.has_value() ? refreshMetrics->sourceFallbackMs : -1)
              << " collect_ms=" << (refreshMetrics.has_value() ? refreshMetrics->featureCollectionMs : -1)
              << " clear_ms=" << (refreshMetrics.has_value() ? refreshMetrics->sceneClearMs : -1)
              << " render_ms=" << (refreshMetrics.has_value() ? refreshMetrics->renderMs : -1)
              << " background_ms=" << (refreshMetrics.has_value() ? refreshMetrics->backgroundRestoreMs : -1)
              << " selection_ms=" << (refreshMetrics.has_value() ? refreshMetrics->selectionMs : -1)
              << " presentation_ms=" << (refreshMetrics.has_value() ? refreshMetrics->presentationMs : -1)
              << " viewport_ms=" << (refreshMetrics.has_value() ? refreshMetrics->viewportMs : -1)
              << " final_ui_ms=" << (refreshMetrics.has_value() ? refreshMetrics->finalUiMs : -1)
              << " refresh_ms=" << refreshMs
              << '\n';

    if (!expect(sceneItems >= 5000, "Large map fixture should render at least 5000 scene items.")) {
        return 1;
    }
    if (!expect(vertexItemsByKey.size() >= 2000, "Large map fixture should index thousands of editable vertices.")) {
        return 1;
    }
    MapEditableGeometryVertexItem *restoredControl = selectedLineControl(scene);
    if (!expect(restoredControl != nullptr
                    && restoredControl->lineNumber() == selectedLineNumber
                    && restoredControl->vertexIndex() == selectedVertexIndex
                    && restoredControl->isVisible(),
                "Large map refresh should restore the selected line control through the vertex index.")) {
        return 1;
    }
    if (!expect(restoredSelection, "Large map refresh should update geometry selection presentation.")) {
        return 1;
    }
    if (!expect(refreshMetrics.has_value()
                    && refreshMetrics->usedProjectionSnapshot
                    && refreshMetrics->projectionSnapshotWasReused
                    && !refreshMetrics->usedSourceFallback,
                "Large map baseline should measure the reused immutable projection path.")) {
        return 1;
    }
    if (!expect(completedSceneGeneration == sceneGeneration.current() && completedSceneGeneration > 0,
                "Large map refresh should publish its current scene generation after selection restoration.")) {
        return 1;
    }
    if (runSelectionPresentationBenchmark(scene, &sceneGeneration, itemsByLine) != 0) {
        return 1;
    }
    scene->clearSelection();

    const QList<QGraphicsItem *> sceneItemsBeforePartialRefresh = scene->items();
    const int targetItemsBefore = std::count_if(sceneItemsBeforePartialRefresh.cbegin(),
                                                sceneItemsBeforePartialRefresh.cend(),
                                                [&targetLine](const QGraphicsItem *item) {
        return item != nullptr
            && item->data(kMapItemRole).toInt() == kMapItemGeometryValue
            && item->data(kMapSceneLineNumberRole).toInt() == targetLine.lineNumber;
    });
    const int targetVertexEntriesBefore = std::count_if(vertexItemsByKey.cbegin(),
                                                         vertexItemsByKey.cend(),
                                                         [&targetLine](const QGraphicsItem *item) {
        return item != nullptr && item->data(kMapSceneLineNumberRole).toInt() == targetLine.lineNumber;
    });
    QElapsedTimer partialTimer;
    partialTimer.start();
    const MapGeometryItemGroupRemovalResult partialRemoval =
        removeMapGeometryItemGroupForLine(scene, targetLine.lineNumber, &itemsByLine, &vertexItemsByKey);
    const qint64 partialRemoveMs = partialTimer.elapsed();
    const MapGeometryItemGroupRenderResult partialRender =
        renderMapGeometryItemGroupForFeature(scene,
                                             targetLine,
                                             geometryBoundsForFeatures(geometryFeatures),
                                             &itemsByLine,
                                             &vertexItemsByKey,
                                             {},
                                             {},
                                             {},
                                             {},
                                             styleCatalog);
    const qint64 partialTotalMs = partialTimer.elapsed();
    std::cout << "large-map-partial-refresh"
              << " line=" << targetLine.lineNumber
              << " items_removed=" << partialRemoval.removedItems
              << " items_added=" << partialRender.addedItems
              << " remove_ms=" << partialRemoveMs
              << " render_ms=" << (partialTotalMs - partialRemoveMs)
              << " total_ms=" << partialTotalMs
              << '\n';
    if (!expect(partialRemoval.removedItems == targetItemsBefore
                    && partialRender.addedItems == targetItemsBefore
                    && partialRender.addedPrimaryItem,
                "Large map partial refresh should replace the complete target line item group.")) {
        return 1;
    }
    if (!expect(std::count_if(vertexItemsByKey.cbegin(),
                              vertexItemsByKey.cend(),
                              [&targetLine](const QGraphicsItem *item) {
            return item != nullptr && item->data(kMapSceneLineNumberRole).toInt() == targetLine.lineNumber;
        }) == targetVertexEntriesBefore,
                "Large map partial refresh should restore the target vertex index entries.")) {
        return 1;
    }

    clearScene();
    delete scene;
    return 0;
}
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    if (runInspectorLineLookupBenchmark() != 0) {
        return 1;
    }
    return runLargeSceneRefreshSmoke();
}
