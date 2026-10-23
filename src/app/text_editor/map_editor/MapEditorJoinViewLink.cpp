#include "MapEditorJoinViewLink.h"

#include "MapEditorTab.h"

#include <QScopedValueRollback>

namespace TherionStudio
{
MapEditorJoinViewLink::MapEditorJoinViewLink(MapEditorTab *first,
                                             MapEditorTab *second,
                                             const Th2SimilarityTransform &secondToFirst,
                                             QObject *parent)
    : QObject(parent)
    , first_(first)
    , second_(second)
    , secondToFirst_(secondToFirst)
    , firstToSecond_(secondToFirst.inverted())
{
    if (first_ != nullptr) {
        connect(first_, &MapEditorTab::mapViewChanged, this, &MapEditorJoinViewLink::alignSecondToFirst);
    }
    if (second_ != nullptr) {
        connect(second_, &MapEditorTab::mapViewChanged, this, &MapEditorJoinViewLink::alignFirstToSecond);
    }
}

bool MapEditorJoinViewLink::connects(const MapEditorTab *a, const MapEditorTab *b) const
{
    return (first_ == a && second_ == b) || (first_ == b && second_ == a);
}

void MapEditorJoinViewLink::showFirstAt(const QPointF &firstCentre, qreal pixelsPerFirstUnit)
{
    if (first_ == nullptr) {
        return;
    }
    {
        const QScopedValueRollback<bool> guard(aligning_, true);
        first_->setMapViewCentreAndScale(firstCentre, pixelsPerFirstUnit);
    }
    // From the requested centre, not the first view's, so the second view is not offset by the first's pixel rounding.
    alignSecondTo(firstCentre);
}

void MapEditorJoinViewLink::alignSecondToFirst()
{
    if (first_ != nullptr) {
        alignSecondTo(first_->mapViewCentreSourcePosition());
    }
}

void MapEditorJoinViewLink::alignSecondTo(const QPointF &firstCentre)
{
    if (aligning_ || first_ == nullptr || second_ == nullptr) {
        return;
    }
    const QScopedValueRollback<bool> guard(aligning_, true);
    // One second-drawing unit spans scale() first-drawing units on screen.
    second_->setMapViewCentreAndScale(firstToSecond_.map(firstCentre),
                                      first_->mapViewPixelsPerSourceUnit() * secondToFirst_.scale());
}

void MapEditorJoinViewLink::alignFirstToSecond()
{
    if (aligning_ || first_ == nullptr || second_ == nullptr) {
        return;
    }
    const QScopedValueRollback<bool> guard(aligning_, true);
    first_->setMapViewCentreAndScale(secondToFirst_.map(second_->mapViewCentreSourcePosition()),
                                     second_->mapViewPixelsPerSourceUnit() / secondToFirst_.scale());
}
}
