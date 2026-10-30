#include "MapEditorSceneHandleIndex.h"

#include "MapEditorPerformanceCounters.h"

#include <QGraphicsItem>
#include <QHash>
#include <QSet>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <unordered_map>

namespace TherionStudio
{
namespace
{
struct Entry
{
    MapEditorSceneHandleKind kind = MapEditorSceneHandleKind::GeometryVertex;
    int lineNumber = 0;
    QPointF key;
    quint64 seq = 0;
    qint64 cell = 0;
    bool inGrid = false;
};

struct SceneIndex
{
    QHash<QGraphicsItem *, Entry> entries;
    QHash<int, QSet<QGraphicsItem *>> byLine;
    QHash<qint64, QVector<QGraphicsItem *>> cells;
    quint64 nextSeq = 1;
    qreal cellSize = 0.0;
    bool gridValid = false;
    int builtCount = 0;
};

using Registry = std::unordered_map<const QGraphicsScene *, std::unique_ptr<SceneIndex>>;

Registry &registry()
{
    static Registry instance;
    return instance;
}

SceneIndex *indexFor(const QGraphicsScene *scene)
{
    if (scene == nullptr) {
        return nullptr;
    }
    const auto it = registry().find(scene);
    return it == registry().end() ? nullptr : it->second.get();
}

bool finitePoint(const QPointF &point)
{
    return std::isfinite(point.x()) && std::isfinite(point.y());
}

int cellCoordinate(qreal value, qreal cellSize)
{
    const qreal scaled = std::floor(value / cellSize);
    constexpr qreal limit = static_cast<qreal>(std::numeric_limits<int>::max() / 2);
    return static_cast<int>(std::clamp(scaled, -limit, limit));
}

qint64 cellKey(int cx, int cy)
{
    return (static_cast<qint64>(cx) << 32) | static_cast<quint32>(cy);
}

void removeFromCell(SceneIndex &index, QGraphicsItem *item, Entry &entry)
{
    if (!entry.inGrid) {
        return;
    }
    const auto it = index.cells.find(entry.cell);
    if (it != index.cells.end()) {
        QVector<QGraphicsItem *> &bucket = it.value();
        const qsizetype position = bucket.indexOf(item);
        if (position >= 0) {
            bucket[position] = bucket.last();
            bucket.removeLast();
        }
        if (bucket.isEmpty()) {
            index.cells.erase(it);
        }
    }
    entry.inGrid = false;
}

void insertIntoCell(SceneIndex &index, QGraphicsItem *item, Entry &entry)
{
    if (!index.gridValid || !finitePoint(entry.key)) {
        entry.inGrid = false;
        return;
    }
    entry.cell = cellKey(cellCoordinate(entry.key.x(), index.cellSize),
                         cellCoordinate(entry.key.y(), index.cellSize));
    index.cells[entry.cell].append(item);
    entry.inGrid = true;
}

// Cell size follows the actual key density of the scene (about four keys per
// occupied-area cell) rather than a fixed TH2 unit assumption.
void rebuildGrid(SceneIndex &index)
{
    index.cells.clear();
    QRectF bounds;
    bool hasBounds = false;
    for (auto it = index.entries.begin(); it != index.entries.end(); ++it) {
        it.value().inGrid = false;
        const QPointF &key = it.value().key;
        if (!finitePoint(key)) {
            continue;
        }
        if (!hasBounds) {
            bounds = QRectF(key, key);
            hasBounds = true;
        } else {
            bounds.setLeft(qMin(bounds.left(), key.x()));
            bounds.setRight(qMax(bounds.right(), key.x()));
            bounds.setTop(qMin(bounds.top(), key.y()));
            bounds.setBottom(qMax(bounds.bottom(), key.y()));
        }
    }
    const int count = qMax(1, static_cast<int>(index.entries.size()));
    const qreal extent = hasBounds ? qMax(bounds.width(), bounds.height()) : 0.0;
    const qreal area = hasBounds ? qMax(bounds.width(), extent / count) * qMax(bounds.height(), extent / count) : 0.0;
    qreal cellSize = area > 0.0 ? std::sqrt(area / count) * 2.0 : 1.0;
    if (!std::isfinite(cellSize) || cellSize <= 0.0) {
        cellSize = 1.0;
    }
    index.cellSize = cellSize;
    index.gridValid = true;
    index.builtCount = static_cast<int>(index.entries.size());
    for (auto it = index.entries.begin(); it != index.entries.end(); ++it) {
        insertIntoCell(index, it.key(), it.value());
    }
}

void noteSizeChange(SceneIndex &index)
{
    const int count = static_cast<int>(index.entries.size());
    if (index.gridValid && (count > (index.builtCount * 2) + 64 || count < index.builtCount / 4)) {
        index.gridValid = false;
        index.cells.clear();
    }
}

QVector<QGraphicsItem *> sortedByStacking(const SceneIndex &index, QVector<QGraphicsItem *> items)
{
    std::sort(items.begin(), items.end(), [&index](QGraphicsItem *a, QGraphicsItem *b) {
        const qreal za = a->zValue();
        const qreal zb = b->zValue();
        if (za != zb) {
            return za > zb;
        }
        return index.entries.value(a).seq > index.entries.value(b).seq;
    });
    return items;
}

bool keyInRect(const QPointF &key, const QRectF &rect)
{
    return key.x() >= rect.left() && key.x() <= rect.right()
        && key.y() >= rect.top() && key.y() <= rect.bottom();
}
}

namespace MapEditorSceneHandleIndex
{
void track(QGraphicsItem *item, MapEditorSceneHandleKind kind, int lineNumber, const QPointF &sceneKey)
{
    if (item == nullptr || item->scene() == nullptr) {
        return;
    }
    std::unique_ptr<SceneIndex> &slot = registry()[item->scene()];
    if (!slot) {
        slot = std::make_unique<SceneIndex>();
    }
    SceneIndex &index = *slot;
    auto existing = index.entries.find(item);
    if (existing != index.entries.end()) {
        removeFromCell(index, item, existing.value());
        index.byLine[existing.value().lineNumber].remove(item);
        index.entries.erase(existing);
    }
    Entry entry;
    entry.kind = kind;
    entry.lineNumber = lineNumber;
    entry.key = sceneKey;
    entry.seq = index.nextSeq++;
    Entry &stored = index.entries.insert(item, entry).value();
    index.byLine[lineNumber].insert(item);
    insertIntoCell(index, item, stored);
    noteSizeChange(index);
}

void untrack(QGraphicsItem *item, const QGraphicsScene *scene)
{
    const auto registryIt = registry().find(scene);
    if (item == nullptr || registryIt == registry().end()) {
        return;
    }
    SceneIndex &index = *registryIt->second;
    const auto it = index.entries.find(item);
    if (it == index.entries.end()) {
        return;
    }
    removeFromCell(index, item, it.value());
    const auto lineIt = index.byLine.find(it.value().lineNumber);
    if (lineIt != index.byLine.end()) {
        lineIt.value().remove(item);
        if (lineIt.value().isEmpty()) {
            index.byLine.erase(lineIt);
        }
    }
    index.entries.erase(it);
    if (index.entries.isEmpty()) {
        registry().erase(registryIt);
        return;
    }
    noteSizeChange(index);
}

void moveKey(QGraphicsItem *item, const QPointF &sceneKey)
{
    SceneIndex *index = item != nullptr ? indexFor(item->scene()) : nullptr;
    if (index == nullptr) {
        return;
    }
    const auto it = index->entries.find(item);
    if (it == index->entries.end() || it.value().key == sceneKey) {
        return;
    }
    removeFromCell(*index, item, it.value());
    it.value().key = sceneKey;
    insertIntoCell(*index, item, it.value());
}

QVector<QGraphicsItem *> itemsInRect(const QGraphicsScene *scene, const QRectF &sceneRect)
{
    ++mapEditorPerformanceCounters().sceneHandleIndexQueries;
    SceneIndex *index = indexFor(scene);
    const QRectF rect = sceneRect.normalized();
    QVector<QGraphicsItem *> result;
    if (index == nullptr || !finitePoint(rect.topLeft()) || !finitePoint(rect.bottomRight())) {
        return result;
    }
    if (!index->gridValid) {
        rebuildGrid(*index);
    }

    const int cx0 = cellCoordinate(rect.left(), index->cellSize);
    const int cx1 = cellCoordinate(rect.right(), index->cellSize);
    const int cy0 = cellCoordinate(rect.top(), index->cellSize);
    const int cy1 = cellCoordinate(rect.bottom(), index->cellSize);
    const double cellCount = (static_cast<double>(cx1) - cx0 + 1.0) * (static_cast<double>(cy1) - cy0 + 1.0);
    int examined = 0;
    if (cellCount > static_cast<double>(index->entries.size())) {
        for (auto it = index->entries.cbegin(); it != index->entries.cend(); ++it) {
            ++examined;
            if (keyInRect(it.value().key, rect)) {
                result.append(it.key());
            }
        }
    } else {
        for (int cx = cx0; cx <= cx1; ++cx) {
            for (int cy = cy0; cy <= cy1; ++cy) {
                const auto bucketIt = index->cells.constFind(cellKey(cx, cy));
                if (bucketIt == index->cells.constEnd()) {
                    continue;
                }
                for (QGraphicsItem *item : bucketIt.value()) {
                    ++examined;
                    if (keyInRect(index->entries.value(item).key, rect)) {
                        result.append(item);
                    }
                }
            }
        }
    }
    mapEditorPerformanceCounters().sceneHandleIndexExaminedEntries += examined;
    return sortedByStacking(*index, std::move(result));
}

QVector<QGraphicsItem *> itemsForLine(const QGraphicsScene *scene, int lineNumber)
{
    ++mapEditorPerformanceCounters().sceneHandleIndexQueries;
    SceneIndex *index = indexFor(scene);
    if (index == nullptr) {
        return {};
    }
    const auto lineIt = index->byLine.constFind(lineNumber);
    if (lineIt == index->byLine.constEnd()) {
        return {};
    }
    QVector<QGraphicsItem *> result(lineIt.value().cbegin(), lineIt.value().cend());
    mapEditorPerformanceCounters().sceneHandleIndexExaminedEntries += static_cast<int>(result.size());
    return sortedByStacking(*index, std::move(result));
}

MapEditorSceneHandleIndexStats stats(const QGraphicsScene *scene)
{
    MapEditorSceneHandleIndexStats result;
    if (SceneIndex *index = indexFor(scene)) {
        if (!index->gridValid) {
            rebuildGrid(*index);
        }
        result.trackedItems = static_cast<int>(index->entries.size());
        result.cellSize = index->cellSize;
    }
    return result;
}
}
}
