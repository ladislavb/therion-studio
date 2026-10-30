#pragma once

#include <QHash>
#include <QSet>
#include <QString>
#include <QVector>

namespace TherionStudio
{
class Th2GeometryProjection;
struct TherionSourceLogicalCommand;

struct MapEditorAreaReference
{
    int areaLineNumber = 0;
    QString areaLabel;
    QString borderLineId;
};

QSet<int> mapEditorBorderLineNumbersForArea(const QString &text, int areaLineNumber);
QSet<int> mapEditorBorderLineNumbersForArea(const QVector<TherionSourceLogicalCommand> &commands, int areaLineNumber);
QSet<int> mapEditorBorderLineNumbersForArea(const Th2GeometryProjection &projection, int areaLineNumber);
QVector<MapEditorAreaReference> mapEditorAreaReferencesForBorderLine(const QString &text, int borderLineNumber);
QVector<MapEditorAreaReference> mapEditorAreaReferencesForBorderLine(const QVector<TherionSourceLogicalCommand> &commands,
                                                                     int borderLineNumber);
QVector<MapEditorAreaReference> mapEditorAreaReferencesForBorderLine(const Th2GeometryProjection &projection,
                                                                     int borderLineNumber);
// Whole-document indexes for callers that need references for many lines (linear instead of per-line rescans).
QHash<int, QVector<MapEditorAreaReference>> mapEditorAreaReferencesByBorderLine(
    const QVector<TherionSourceLogicalCommand> &commands);
QHash<int, QVector<MapEditorAreaReference>> mapEditorAreaReferencesByBorderLine(const Th2GeometryProjection &projection);
}
