#include "MapEditorSvgBackgroundItem.h"

#include <QPainter>
#include <QPixmap>
#include <QSvgRenderer>

namespace TherionStudio
{
namespace
{
constexpr int kMapEditorSvgBackgroundItemRole = 118;
}

MapEditorSvgBackgroundItem::MapEditorSvgBackgroundItem(const QByteArray &svgData,
                                                       const QSizeF &intrinsicSize,
                                                       const QRectF &sourceViewBox)
    : intrinsicSize_(intrinsicSize)
    , sourceViewBox_(sourceViewBox)
    , renderer_(new QSvgRenderer(svgData, nullptr))
{
    setData(kMapEditorSvgBackgroundItemRole, true);
    const QRectF defaultViewBox(QPointF(0.0, 0.0), intrinsicSize_);
    if (renderer_->isValid() && sourceViewBox_.isValid() && sourceViewBox_ != defaultViewBox) {
        renderer_->setViewBox(sourceViewBox_);
    }

    const QSize pixmapSize(qMax(1, qRound(intrinsicSize_.width())),
                           qMax(1, qRound(intrinsicSize_.height())));
    QPixmap backing(pixmapSize);
    backing.fill(Qt::transparent);
    setPixmap(backing);
    setShapeMode(QGraphicsPixmapItem::BoundingRectShape);
}

MapEditorSvgBackgroundItem::~MapEditorSvgBackgroundItem()
{
    delete renderer_;
}

bool MapEditorSvgBackgroundItem::isValid() const
{
    return renderer_ != nullptr
        && renderer_->isValid()
        && intrinsicSize_.isValid()
        && intrinsicSize_.width() > 0.0
        && intrinsicSize_.height() > 0.0;
}

QSizeF MapEditorSvgBackgroundItem::intrinsicSize() const
{
    return intrinsicSize_;
}

QRectF MapEditorSvgBackgroundItem::sourceViewBox() const
{
    return sourceViewBox_;
}

void MapEditorSvgBackgroundItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)

    if (painter == nullptr || !isValid()) {
        return;
    }

    painter->save();
    renderer_->render(painter, QRectF(QPointF(0.0, 0.0), intrinsicSize_));
    painter->restore();
}

bool isMapEditorSvgBackgroundItem(const QGraphicsPixmapItem *item)
{
    return item != nullptr && item->data(kMapEditorSvgBackgroundItemRole).toBool();
}

}
