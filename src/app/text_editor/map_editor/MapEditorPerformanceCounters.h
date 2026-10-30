#pragma once

#include <atomic>

namespace TherionStudio
{
// Monotonic work counters for hot interaction paths. They are always compiled
// (relaxed atomics are negligible next to the work they count) so regression
// tests can assert that expensive work is not repeated per event or per row.
struct MapEditorPerformanceCounters final
{
    // MapEditorViewportInputController::handleEvent calls for pointer events
    // delivered to the map viewport (not counting tablet->mouse forwarding).
    std::atomic<int> viewportPointerDispatches{0};
    // Whole-document collectGeometryFeatures(parsedLines) builds.
    std::atomic<int> wholeDocumentGeometryBuilds{0};
    // Inspector SVG icon rasterisations.
    std::atomic<int> inspectorIconRenders{0};
    // Select-mode hover hit tests (preferredMapHitItemForViewportPosition).
    std::atomic<int> hoverHitTests{0};
    // Uncached interactive-draft source bounds computations (text re-parse).
    std::atomic<int> draftSourceBoundsComputations{0};
    // Whole-scene QGraphicsScene::items() walks on interaction paths
    // (hit-testing, snapping, selection bookkeeping).
    std::atomic<int> fullSceneItemScans{0};
    // Per-scene-generation item index rebuilds (whole-scene walk, amortised).
    std::atomic<int> sceneRevisionIndexRebuilds{0};
    // Spatial handle-index queries (rect or per-line).
    std::atomic<int> sceneHandleIndexQueries{0};
    // Index entries examined by those queries (candidate-count metric).
    std::atomic<int> sceneHandleIndexExaminedEntries{0};
    // MapEditorXviBackgroundItem sketch rasterisations (vector paint passes).
    std::atomic<int> xviBackgroundRenders{0};
    // Slope decoration source metrics computed during paint.
    std::atomic<int> lineDecorationSourceMetricComputations{0};
    std::atomic<int> lineDecorationProjectionComputations{0};
    std::atomic<int> screenSpaceMetricPasses{0};
    std::atomic<int> rasterGammaJobsScheduled{0};
};

inline MapEditorPerformanceCounters &mapEditorPerformanceCounters()
{
    static MapEditorPerformanceCounters counters;
    return counters;
}
}
