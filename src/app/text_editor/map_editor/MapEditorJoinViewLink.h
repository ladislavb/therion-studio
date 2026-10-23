#pragma once

#include "../../../core/Th2CentrelineAlignment.h"

#include <QObject>
#include <QPointer>

namespace TherionStudio
{
class MapEditorTab;

// Keeps two map editors showing the same place at the same real-world scale, using the transform
// that maps second-drawing coordinates into first-drawing coordinates. Rotation between the
// drawings is not applied to the views; Therion scraps of one projection share orientation.
class MapEditorJoinViewLink final : public QObject
{
    Q_OBJECT

public:
    MapEditorJoinViewLink(MapEditorTab *first,
                          MapEditorTab *second,
                          const Th2SimilarityTransform &secondToFirst,
                          QObject *parent = nullptr);

    // True when this link joins `a` and `b`, in either order.
    [[nodiscard]] bool connects(const MapEditorTab *a, const MapEditorTab *b) const;
    // Shows `firstCentre` in the first editor at `pixelsPerFirstUnit`, then aligns the second editor.
    void showFirstAt(const QPointF &firstCentre, qreal pixelsPerFirstUnit);

private:
    void alignSecondToFirst();
    // Centres the second editor on the point matching `firstCentre`.
    void alignSecondTo(const QPointF &firstCentre);
    void alignFirstToSecond();

    QPointer<MapEditorTab> first_;
    QPointer<MapEditorTab> second_;
    Th2SimilarityTransform secondToFirst_;
    Th2SimilarityTransform firstToSecond_;
    bool aligning_ = false;
};
}
