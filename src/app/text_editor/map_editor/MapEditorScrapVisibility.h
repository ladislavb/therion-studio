#pragma once

#include <QSet>
#include <QVector>

#include <optional>

namespace TherionStudio
{
class Th2GeometryProjection;

// How a map object or scrap is drawn. View state only; never written to the source.
enum class MapEditorObjectVisibility
{
    Shown,
    Faded,
    Hidden
};

enum class MapEditorScrapVisibilityTarget
{
    ThisScrap,
    OtherScraps
};

inline constexpr qreal kMapEditorFadedObjectOpacity = 0.25;

// Shown -> faded -> hidden -> shown, as the Objects view eye icon cycles.
[[nodiscard]] MapEditorObjectVisibility nextMapEditorObjectVisibility(MapEditorObjectVisibility visibility);

// Non-owning view over the hidden and faded source line sets kept by the map tab.
class MapEditorObjectVisibilityLines final
{
public:
    MapEditorObjectVisibilityLines(QSet<int> *hiddenLines, QSet<int> *fadedLines);

    [[nodiscard]] MapEditorObjectVisibility visibility(int lineNumber) const;
    [[nodiscard]] bool allShown() const;
    void setVisibility(const QVector<int> &lineNumbers, MapEditorObjectVisibility visibility);
    void remove(int lineNumber);
    void clear();
    void retainOnly(const QSet<int> &lineNumbers);

private:
    QSet<int> *hiddenLines_ = nullptr;
    QSet<int> *fadedLines_ = nullptr;
};

// A scrap's own line followed by the lines of the points, lines and areas inside it.
struct MapEditorScrapObjectLines
{
    int scrapLineNumber = 0;
    QVector<int> lineNumbers;
};

[[nodiscard]] QVector<MapEditorScrapObjectLines> mapEditorScrapObjectLines(const Th2GeometryProjection &projection);

// The scrap line for a scrap or an object inside one, or 0 when the line is outside every scrap.
[[nodiscard]] int mapEditorScrapLineForLine(const QVector<MapEditorScrapObjectLines> &scraps, int lineNumber);

// The visibility shared by every line of the target scraps, or none when they differ or there are no such scraps.
[[nodiscard]] std::optional<MapEditorObjectVisibility> mapEditorScrapsVisibility(
    const MapEditorObjectVisibilityLines &visibilityLines,
    const QVector<MapEditorScrapObjectLines> &scraps,
    int scrapLineNumber,
    MapEditorScrapVisibilityTarget target);

[[nodiscard]] bool mapEditorHasOtherScraps(const QVector<MapEditorScrapObjectLines> &scraps, int scrapLineNumber);

void setMapEditorScrapsVisibility(MapEditorObjectVisibilityLines &visibilityLines,
                                  const QVector<MapEditorScrapObjectLines> &scraps,
                                  int scrapLineNumber,
                                  MapEditorScrapVisibilityTarget target,
                                  MapEditorObjectVisibility visibility);
}
