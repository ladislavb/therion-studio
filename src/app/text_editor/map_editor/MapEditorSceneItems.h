#pragma once

#include <QGraphicsScene>
#include <QList>

namespace TherionStudio
{
// QGraphicsScene::items() sorts every item by stacking order, which dominates full-scene
// scans of large maps. Qt skips sorting for Qt::SortOrder(-1); use this only where the
// iteration order does not matter.
inline QList<QGraphicsItem *> unsortedSceneItems(const QGraphicsScene *scene)
{
    return scene != nullptr ? scene->items(Qt::SortOrder(-1)) : QList<QGraphicsItem *>{};
}
}
