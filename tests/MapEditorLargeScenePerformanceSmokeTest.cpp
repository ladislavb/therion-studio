#include "../src/app/text_editor/map_editor/MapEditorSceneInternals.h"
#include "../src/app/text_editor/map_editor/MapEditorSceneRefreshController.h"
#include "../src/app/text_editor/map_editor/MapEditorSceneSupport.h"
#include "../src/app/text_editor/map_editor/MapEditorObjectStyleCatalog.h"
#include "../src/core/TherionDocumentParser.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QHash>
#include <QScrollBar>
#include <QString>
#include <QUndoStack>
#include <QVector>

#include <iostream>
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
    int selectedLineNumber = targetLine.lineNumber;
    int selectedVertexIndex = targetLine.lineVertices.first().outgoingSourceVertexIndex;
    QString selectedKind = QStringLiteral("line control");
    int currentLineNumber = targetLine.lineNumber;
    bool restoredSelection = false;

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

    clearScene();
    delete scene;
    return 0;
}
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    return runLargeSceneRefreshSmoke();
}
