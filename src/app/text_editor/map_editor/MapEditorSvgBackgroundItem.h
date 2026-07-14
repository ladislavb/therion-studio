#pragma once

#include <QByteArray>
#include <QGraphicsPixmapItem>
#include <QRectF>
#include <QSizeF>
#include <QString>

class QSvgRenderer;

namespace TherionStudio
{

class MapEditorSvgBackgroundItem final : public QGraphicsPixmapItem
{
public:
    explicit MapEditorSvgBackgroundItem(const QByteArray &svgData,
                                        const QSizeF &intrinsicSize,
                                        const QRectF &sourceViewBox);
    ~MapEditorSvgBackgroundItem() override;

    bool isValid() const;
    bool reloadSourceData(const QByteArray &svgData);
    QSizeF intrinsicSize() const;
    QRectF sourceViewBox() const;

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    QSizeF intrinsicSize_;
    QRectF sourceViewBox_;
    QSvgRenderer *renderer_ = nullptr;
};

bool isMapEditorSvgBackgroundItem(const QGraphicsPixmapItem *item);

}
