#pragma once

#include "../../../core/Th2CentrelineAlignment.h"

#include <QGraphicsObject>
#include <QObject>
#include <QPainterPath>
#include <QPointer>
#include <QTimer>

namespace TherionStudio
{
class MapEditorTab;

// Faded, display-only outline of another drawing. Ignores the mouse and never edits anything.
class MapEditorJoinOutlineItem final : public QGraphicsObject
{
    Q_OBJECT

public:
    explicit MapEditorJoinOutlineItem(QGraphicsItem *parent = nullptr);

    void setOutlinePath(const QPainterPath &path);
    [[nodiscard]] const QPainterPath &outlinePath() const;
    [[nodiscard]] QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    QPainterPath path_;
};

// Shows `partner`'s drawing in `host`, placed by `partnerToHost` (partner drawing units to host drawing
// units). Owned by `host`; rebuilds after any partner source change (debounced) and re-adds its item after
// host scene refreshes.
class MapEditorJoinOutline final : public QObject
{
    Q_OBJECT

public:
    // Replaces any outline of the same partner already shown in `host`.
    static MapEditorJoinOutline *attach(MapEditorTab *host,
                                        MapEditorTab *partner,
                                        const Th2SimilarityTransform &partnerToHost);

    [[nodiscard]] static MapEditorJoinOutline *find(MapEditorTab *host, const MapEditorTab *partner);

    void setOutlineVisible(bool visible);
    [[nodiscard]] bool isOutlineVisible() const;
    [[nodiscard]] MapEditorTab *partner() const;
    [[nodiscard]] MapEditorJoinOutlineItem *item() const;

private:
    MapEditorJoinOutline(MapEditorTab *host, MapEditorTab *partner, const Th2SimilarityTransform &partnerToHost);

    void rebuildPath();
    void placeItem();

    QPointer<MapEditorTab> host_;
    QPointer<MapEditorTab> partner_;
    QPointer<MapEditorJoinOutlineItem> item_;
    QTransform partnerToHost_;
    QTimer partnerChangeDebounce_;
    bool visible_ = true;
};
}
