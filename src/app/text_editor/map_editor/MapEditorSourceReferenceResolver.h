#pragma once

#include "MapEditorSceneSupport.h"

#include <QPointF>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

#include <optional>

namespace TherionStudio
{
struct TherionParsedLine;
struct TherionSourceLogicalCommand;
class Th2GeometryProjection;

struct SourceVertexTextReference
{
    int lineNumber = 0;
    int sourceVertexIndex = -1;
    int xStartColumn = 1;
    int xEndColumn = 1;
    int yStartColumn = 1;
    int yEndColumn = 1;
};

struct CursorGeometrySelection
{
    int featureLineNumber = 0;
    QString geometryKind;
    std::optional<SourceVertexTextReference> sourceVertexReference;
};

std::optional<MapGeometryFeature> lineFeatureForLineNumber(const QString &documentText, int lineNumber);
std::optional<MapGeometryFeature> lineFeatureForLineNumber(const QVector<TherionParsedLine> &parsedLines, int lineNumber);
std::optional<MapGeometryFeature> lineFeatureForLineNumber(const QVector<TherionSourceLogicalCommand> &commands,
                                                           int lineNumber);
std::optional<MapGeometryFeature> lineFeatureForLineNumber(const Th2GeometryProjection &projection,
                                                           const QVector<TherionSourceLogicalCommand> &commands,
                                                           int lineNumber);
std::optional<MapGeometryFeature> geometryFeatureForLineNumber(const QVector<TherionSourceLogicalCommand> &commands,
                                                               int lineNumber,
                                                               MapGeometryFeature::Kind expectedKind);
QString formatSourceCoordinate(qreal value);
QStringList coordinateRowsForLineVertices(const QVector<MapGeometryFeature::TH2LineVertex> &lineVertices,
                                          bool closed = false);
CursorGeometrySelection cursorGeometrySelectionForTextCursor(const QVector<TherionParsedLine> &parsedLines,
                                                             int cursorLine,
                                                             int cursorColumn);
QString mapObjectKindForSourceLine(const Th2GeometryProjection &projection, int lineNumber);
std::optional<SourceVertexTextReference> sourceVertexTextReferenceForSelection(const QVector<TherionParsedLine> &parsedLines,
                                                                               int featureLineNumber,
                                                                               const QString &geometryKind,
                                                                               int sourceVertexIndex);
std::optional<QSet<int>> scrapObjectLinesForCursor(const QVector<TherionParsedLine> &parsedLines,
                                                   int cursorLine);
std::optional<int> sourcePointLineNumberForSelection(const QVector<TherionParsedLine> &parsedLines,
                                                     const QPointF &sourcePoint);
std::optional<int> sourcePointLineNumberForSelection(const Th2GeometryProjection &projection,
                                                     const QPointF &sourcePoint);
}
