#pragma once

#include <QGraphicsLineItem>
#include <QTransform>

#include <cmath>

namespace TherionStudio
{
class MapEditorDirectionAidItem final : public QGraphicsLineItem
{
public:
    MapEditorDirectionAidItem()
    {
        setFlag(ItemIgnoresTransformations);
    }

    void setSceneLine(const QLineF &line)
    {
        sceneLine_ = line;
        setPos(line.p1());
        updateViewTransform(viewTransform_);
    }

    void updateViewTransform(const QTransform &transform)
    {
        viewTransform_ = transform;
        const QPointF delta = transform.map(sceneLine_.p2()) - transform.map(sceneLine_.p1());
        const qreal length = std::hypot(delta.x(), delta.y());
        setLine(QLineF(QPointF(), length > 1e-9
            ? delta * (sceneLine_.length() / length) : QPointF()));
    }

private:
    QLineF sceneLine_;
    QTransform viewTransform_;
};
}
