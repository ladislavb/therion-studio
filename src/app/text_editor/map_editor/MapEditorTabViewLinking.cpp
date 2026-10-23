#include "MapEditorTab.h"

#include "MapEditorBackgroundAssetLoader.h"
#include "MapEditorSceneSupport.h"
#include "../TextEditorTab.h"
#include "../../../core/TherionBackgroundMetadata.h"

#include <QGraphicsScene>
#include <QGraphicsView>
#include <QLineF>

#include <cmath>

namespace TherionStudio
{
namespace
{
bool sourceProjectionUsable(const QRectF &sourceBounds, const QRectF &previewBounds)
{
    return previewBounds.isValid() && sourceBounds.isValid() && sourceBounds.width() >= 0.001
        && sourceBounds.height() >= 0.001;
}
}

QPointF MapEditorTab::mapViewCentreSourcePosition() const
{
    if (mapView_ == nullptr || mapView_->viewport() == nullptr) {
        return QPointF();
    }
    const QPointF sceneCentre = mapView_->mapToScene(mapView_->viewport()->rect().center());
    const QRectF sourceBounds = mapSourceBoundsForCurrentDocument();
    const QRectF previewBounds = mapPreviewBounds();
    // Inverse of scenePointFromSourcePosition, using the same bounds.
    return sourceProjectionUsable(sourceBounds, previewBounds)
        ? mapGeometryPreviewToSource(sceneCentre, sourceBounds, previewBounds)
        : sceneCentre;
}

qreal MapEditorTab::mapViewPixelsPerSourceUnit() const
{
    if (mapView_ == nullptr) {
        return 0.0;
    }
    const QPointF origin = mapViewCentreSourcePosition();
    const qreal sceneUnitsPerSourceUnit =
        QLineF(scenePointFromSourcePosition(origin), scenePointFromSourcePosition(origin + QPointF(1.0, 0.0))).length();
    const QTransform viewTransform = mapView_->transform();
    return std::hypot(viewTransform.m11(), viewTransform.m21()) * sceneUnitsPerSourceUnit;
}

int MapEditorTab::mapViewportPixelWidth() const
{
    return mapView_ != nullptr && mapView_->viewport() != nullptr ? mapView_->viewport()->width() : 0;
}

void MapEditorTab::setMapViewCentreAndScale(const QPointF &sourceCentre, qreal pixelsPerSourceUnit)
{
    if (mapView_ == nullptr || pixelsPerSourceUnit <= 0.0) {
        return;
    }
    const qreal currentPixelsPerSourceUnit = mapViewPixelsPerSourceUnit();
    if (currentPixelsPerSourceUnit > 0.0 && !qFuzzyCompare(currentPixelsPerSourceUnit, pixelsPerSourceUnit)) {
        // Shared zoom path: keeps zoom limits, auto-fit state, and zoom status in step.
        adjustMapZoom(pixelsPerSourceUnit / currentPixelsPerSourceUnit);
    }
    const QPointF sceneCentre = scenePointFromSourcePosition(sourceCentre);
    ensureMapViewCanCentreOn(sceneCentre);
    mapView_->centerOn(sceneCentre);
}

void MapEditorTab::ensureMapViewCanCentreOn(const QPointF &sceneCentre)
{
    if (mapView_ == nullptr || mapScene_ == nullptr || mapView_->viewport() == nullptr) {
        return;
    }
    // A linked view must be able to centre any point, including one at the drawing's edge. Widen the
    // view's own scroll area rather than the scene rect, which scene refreshes own.
    const QRectF visibleScene = mapView_->mapToScene(mapView_->viewport()->rect()).boundingRect();
    QRectF needed(QPointF(), visibleScene.size());
    needed.moveCenter(sceneCentre);
    const QRectF current = mapViewScrollAreaExtended_ ? mapView_->sceneRect() : mapScene_->sceneRect();
    if (current.contains(needed)) {
        return;
    }
    if (!mapViewScrollAreaExtended_) {
        mapViewScrollAreaExtended_ = true;
        connect(mapScene_, &QGraphicsScene::sceneRectChanged, this, [this](const QRectF &sceneRect) {
            if (mapView_ != nullptr) {
                mapView_->setSceneRect(mapView_->sceneRect().united(sceneRect));
            }
        });
    }
    mapView_->setSceneRect(current.united(needed));
}

Th2CentrelineFit MapEditorTab::centrelineFitForSourceLine(int lineNumber)
{
    const MapEditorSourceProjectionSnapshotPtr snapshot = sourceProjectionSnapshotForCurrentDocument();
    if (snapshot == nullptr || textEditor_ == nullptr) {
        return Th2CentrelineFit();
    }
    QVector<TherionXviDocument> xviDocuments;
    for (const TherionBackgroundReference &reference :
         parseTherionBackgroundReferences(textEditor_->text(), textEditor_->filePath())) {
        TherionXviDocument xviDocument;
        if (reference.xviReference
            && MapEditorBackgroundAssetLoader::loadXvi(backgroundAssetCache_, reference.absolutePath, &xviDocument)) {
            xviDocuments.append(xviDocument);
        }
    }
    return fitTh2ScrapContainingLine(snapshot->logicalCommands, lineNumber, xviDocuments);
}

std::optional<QPointF> MapEditorTab::stationPointSourcePosition(int lineNumber) const
{
    const MapEditorSourceProjectionSnapshotPtr snapshot = sourceProjectionSnapshotForCurrentDocument();
    if (snapshot == nullptr) {
        return std::nullopt;
    }
    const std::optional<Th2ScrapStationPoint> point = th2StationPointAtLine(snapshot->logicalCommands, lineNumber);
    return point.has_value() ? std::optional<QPointF>(point->position) : std::nullopt;
}

Th2CentrelineFit MapEditorTab::measuredLegFitForSourceLine(int lineNumber,
                                                          const QHash<int, QString> &stationKeysByLine,
                                                          const QVector<Th2MeasuredLeg> &legs,
                                                          const QString &projection) const
{
    const MapEditorSourceProjectionSnapshotPtr snapshot = sourceProjectionSnapshotForCurrentDocument();
    if (snapshot == nullptr) {
        return Th2CentrelineFit();
    }
    return fitTh2ScrapToMeasuredLegs(snapshot->logicalCommands, lineNumber, stationKeysByLine, legs, projection);
}

QVector<Th2ScrapStationPoint> MapEditorTab::scrapStationPointsForSourceLine(int lineNumber) const
{
    const MapEditorSourceProjectionSnapshotPtr snapshot = sourceProjectionSnapshotForCurrentDocument();
    if (snapshot == nullptr) {
        return {};
    }
    const int scrapStartLine = th2ScrapStartLineContaining(snapshot->logicalCommands, lineNumber);
    return scrapStartLine > 0 ? th2ScrapStationPoints(snapshot->logicalCommands, scrapStartLine)
                              : QVector<Th2ScrapStationPoint>();
}

QPainterPath MapEditorTab::mapOutlineScenePath() const
{
    const MapEditorSourceProjectionSnapshotPtr snapshot = sourceProjectionSnapshotForCurrentDocument();
    const QRectF previewBounds = mapPreviewBounds();
    if (snapshot == nullptr || !sourceProjectionUsable(snapshot->sourceBounds, previewBounds)) {
        return QPainterPath();
    }
    constexpr qreal kStationMarkerRadius = 3.0;
    QPainterPath outline;
    for (const MapGeometryFeature &feature :
         collectGeometryFeatures(snapshot->geometryProjection, snapshot->logicalCommands)) {
        if (feature.kind == MapGeometryFeature::Kind::Line) {
            outline.addPath(linePathForFeature(feature, snapshot->sourceBounds, previewBounds));
        } else if (feature.stationPoint && feature.hasSourceAnchor) {
            outline.addEllipse(mapGeometryPointToPreview(feature.sourceAnchor, snapshot->sourceBounds, previewBounds),
                               kStationMarkerRadius,
                               kStationMarkerRadius);
        }
    }
    return outline;
}

QTransform MapEditorTab::mapSourceToSceneTransform() const
{
    // The source-to-scene mapping is affine (uniform scale, y flip, shift); recover it from three points.
    const QPointF origin = scenePointFromSourcePosition(QPointF(0.0, 0.0));
    const QPointF unitX = scenePointFromSourcePosition(QPointF(1.0, 0.0)) - origin;
    const QPointF unitY = scenePointFromSourcePosition(QPointF(0.0, 1.0)) - origin;
    return QTransform(unitX.x(), unitX.y(), unitY.x(), unitY.y(), origin.x(), origin.y());
}

void MapEditorTab::addMapOverlayItem(QGraphicsItem *item)
{
    if (mapScene_ != nullptr && item != nullptr && item->scene() != mapScene_) {
        mapScene_->addItem(item);
    }
}
}
