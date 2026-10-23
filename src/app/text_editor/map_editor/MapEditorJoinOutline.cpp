#include "MapEditorJoinOutline.h"

#include "MapEditorTab.h"

#include <QPainter>

namespace TherionStudio
{
namespace
{
constexpr qreal kJoinOutlineZValue = 2.0; // above backgrounds, below the drawing's own lines (2.5)
constexpr int kPartnerChangeDebounceMs = 150;
const QColor kJoinOutlineColor(128, 128, 128, 150);

QTransform qTransformFromSimilarity(const Th2SimilarityTransform &similarity)
{
    return QTransform(similarity.a, similarity.b, -similarity.b, similarity.a, similarity.tx, similarity.ty);
}
}

MapEditorJoinOutlineItem::MapEditorJoinOutlineItem(QGraphicsItem *parent)
    : QGraphicsObject(parent)
{
    setAcceptedMouseButtons(Qt::NoButton);
    setAcceptHoverEvents(false);
    setZValue(kJoinOutlineZValue);
}

void MapEditorJoinOutlineItem::setOutlinePath(const QPainterPath &path)
{
    prepareGeometryChange();
    path_ = path;
}

const QPainterPath &MapEditorJoinOutlineItem::outlinePath() const
{
    return path_;
}

QRectF MapEditorJoinOutlineItem::boundingRect() const
{
    return path_.boundingRect();
}

void MapEditorJoinOutlineItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    QPen pen(kJoinOutlineColor, 1.5);
    pen.setCosmetic(true);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(path_);
}

MapEditorJoinOutline *MapEditorJoinOutline::attach(MapEditorTab *host,
                                                   MapEditorTab *partner,
                                                   const Th2SimilarityTransform &partnerToHost)
{
    if (host == nullptr || partner == nullptr || host == partner) {
        return nullptr;
    }
    delete find(host, partner);
    return new MapEditorJoinOutline(host, partner, partnerToHost);
}

MapEditorJoinOutline *MapEditorJoinOutline::find(MapEditorTab *host, const MapEditorTab *partner)
{
    if (host == nullptr) {
        return nullptr;
    }
    for (MapEditorJoinOutline *outline : host->findChildren<MapEditorJoinOutline *>(Qt::FindDirectChildrenOnly)) {
        if (outline->partner() == partner) {
            return outline;
        }
    }
    return nullptr;
}

MapEditorJoinOutline::MapEditorJoinOutline(MapEditorTab *host,
                                           MapEditorTab *partner,
                                           const Th2SimilarityTransform &partnerToHost)
    : QObject(host)
    , host_(host)
    , partner_(partner)
    , partnerToHost_(qTransformFromSimilarity(partnerToHost))
{
    // Some map edits (e.g. moving a vertex of a plain line) change the source without a scene refresh, so follow
    // the partner's text; the outline is built from its current source snapshot, not its scene.
    partnerChangeDebounce_.setSingleShot(true);
    partnerChangeDebounce_.setInterval(kPartnerChangeDebounceMs);
    connect(&partnerChangeDebounce_, &QTimer::timeout, this, &MapEditorJoinOutline::rebuildPath);
    connect(partner, &MapEditorTab::documentTextChanged, &partnerChangeDebounce_, qOverload<>(&QTimer::start));
    connect(partner, &MapEditorTab::sceneProjectionRefreshCompleted, this, &MapEditorJoinOutline::rebuildPath);
    connect(host, &MapEditorTab::sceneProjectionRefreshCompleted, this, &MapEditorJoinOutline::placeItem);
    // The outline belongs to the pair: closing the partner removes it.
    connect(partner, &QObject::destroyed, this, &QObject::deleteLater);
    rebuildPath();
}

void MapEditorJoinOutline::setOutlineVisible(bool visible)
{
    visible_ = visible;
    if (item_ != nullptr) {
        item_->setVisible(visible_);
    }
}

bool MapEditorJoinOutline::isOutlineVisible() const
{
    return visible_;
}

MapEditorTab *MapEditorJoinOutline::partner() const
{
    return partner_;
}

MapEditorJoinOutlineItem *MapEditorJoinOutline::item() const
{
    return item_;
}

void MapEditorJoinOutline::rebuildPath()
{
    if (partner_ == nullptr) {
        return;
    }
    if (item_ == nullptr) {
        item_ = new MapEditorJoinOutlineItem();
        // The item is owned by the host scene; when a full refresh deletes it, QPointer clears.
        connect(this, &QObject::destroyed, item_, &QObject::deleteLater);
    }
    item_->setOutlinePath(partner_->mapOutlineScenePath());
    placeItem();
}

void MapEditorJoinOutline::placeItem()
{
    if (host_ == nullptr || partner_ == nullptr) {
        return;
    }
    if (item_ == nullptr) {
        rebuildPath();
        return;
    }
    // Partner scene -> partner drawing units -> host drawing units -> host scene.
    item_->setTransform(partner_->mapSourceToSceneTransform().inverted() * partnerToHost_
                        * host_->mapSourceToSceneTransform());
    item_->setVisible(visible_);
    host_->addMapOverlayItem(item_);
}
}
