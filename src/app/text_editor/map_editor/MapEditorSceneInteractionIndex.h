#pragma once

#include <QHash>
#include <QSet>
#include <QVector>
#include <QtGlobal>

class QGraphicsItem;
class QGraphicsScene;

namespace TherionStudio
{
class MapEditorSceneInteractionIndex final
{
public:
    using ItemsByLine = QHash<int, QVector<QGraphicsItem *>>;

    const ItemsByLine &itemsByLine(QGraphicsScene *scene, quint64 generation);
    // Station point lines grouped by owning scrap line, and the reverse
    // lookup; built in the same per-generation pass as itemsByLine().
    const QHash<int, QVector<int>> &stationLinesByScrap(QGraphicsScene *scene, quint64 generation);
    int stationScrapLine(QGraphicsScene *scene, quint64 generation, int stationLine);
    void invalidate();

    // Lines whose items currently carry selection presentation (highlight,
    // focused vertex or visible selection-gated handles). Owned here rather
    // than by the per-call selection controller so delta updates can clear
    // the previously presented lines.
    bool hasSelectionPresentation(QGraphicsScene *scene, quint64 generation) const;
    const QSet<int> &presentedSelectionLines() const;
    void recordSelectionPresentation(QGraphicsScene *scene, quint64 generation, const QSet<int> &lines);

private:
    QGraphicsScene *indexedScene_ = nullptr;
    quint64 indexedGeneration_ = 0;
    bool valid_ = false;
    ItemsByLine itemsByLine_;
    QHash<int, QVector<int>> stationLinesByScrap_;
    QHash<int, int> stationScrapByLine_;
    QGraphicsScene *presentationScene_ = nullptr;
    quint64 presentationGeneration_ = 0;
    bool presentationValid_ = false;
    QSet<int> presentedSelectionLines_;
};
}
