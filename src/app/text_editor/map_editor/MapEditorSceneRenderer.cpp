#include "MapEditorSceneSupport.h"
#include "MapEditorSceneInternals.h"
#include "MapEditorObjectStyleCatalog.h"

#include <QFont>
#include <QGuiApplication>
#include <QGraphicsLineItem>
#include <QGraphicsPathItem>
#include <QGraphicsScene>
#include <QPalette>
#include <QPainter>
#include <QPainterPath>
#include <QRegularExpression>
#include <QStyleHints>

#include "../../../core/TherionDocumentParser.h"

#include <cmath>
#include <limits>
#include <memory>

namespace TherionStudio {
namespace {
constexpr int kMapItemRole = Qt::UserRole + 120;
constexpr int kMapItemGeometryValue = 1;

struct LineControlConnectorBinding
{
    int anchorVertexOrder = -1;
    int controlSourceVertexIndex = -1;
    QGraphicsLineItem *lineItem = nullptr;
};

struct MapCanvasTheme
{
    bool lightMode = false;
    QColor canvasBorder;
    QColor canvasFill;
    QColor gridLine;
    QColor geometryStroke;
    QColor areaFill;
    QColor labelText;
    QColor mutedText;
    QColor pointHandleStroke;
    QColor pointHandleFill;
    QColor controlConnector;
    QColor controlHandleStroke;
    QColor controlHandleFill;
    QColor stationMarker;
};

template <typename T>
T *makeMouseTransparent(T *item)
{
    if (item != nullptr) {
        item->setAcceptedMouseButtons(Qt::NoButton);
        item->setFlag(QGraphicsItem::ItemIsSelectable, false);
    }
    return item;
}

QPen cosmeticPen(const QColor &color,
                 qreal width,
                 Qt::PenStyle style = Qt::SolidLine,
                 Qt::PenCapStyle cap = Qt::SquareCap,
                 Qt::PenJoinStyle join = Qt::BevelJoin)
{
    QPen pen(color, width, style, cap, join);
    pen.setCosmetic(true);
    return pen;
}

QPen geometricPen(const QColor &color,
                  qreal width,
                  Qt::PenStyle style = Qt::SolidLine,
                  Qt::PenCapStyle cap = Qt::SquareCap,
                  Qt::PenJoinStyle join = Qt::BevelJoin)
{
    QPen pen(color, width, style, cap, join);
    pen.setCosmetic(false);
    return pen;
}

qreal zoomOutStrokeScale(qreal lod)
{
    if (lod >= 1.0) {
        return 1.0;
    }

    return qBound<qreal>(0.32, std::pow(qMax<qreal>(0.01, lod), 0.72), 1.0);
}

class MapZoomAwarePathItem final : public QGraphicsPathItem
{
public:
    MapZoomAwarePathItem(const QPainterPath &path,
                         const QPen &basePen,
                         const QBrush &brush = Qt::NoBrush,
                         QGraphicsItem *parent = nullptr)
        : QGraphicsPathItem(path, parent)
        , basePen_(basePen)
    {
        setPen(basePen_);
        setBrush(brush);
    }

protected:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override
    {
        Q_UNUSED(widget);

        if (painter == nullptr) {
            return;
        }

        const qreal lod = option != nullptr
            ? QStyleOptionGraphicsItem::levelOfDetailFromTransform(painter->worldTransform())
            : 1.0;
        const qreal widthScale = zoomOutStrokeScale(lod);

        QPen drawPen = basePen_;
        drawPen.setCosmetic(true);
        drawPen.setWidthF(qMax<qreal>(0.15, basePen_.widthF() * widthScale));

        painter->save();
        painter->setPen(drawPen);
        painter->setBrush(brush());
        painter->drawPath(path());
        painter->restore();
    }

private:
    QPen basePen_;
};

QPen styledGeometricPen(const QColor &color,
                        qreal width,
                        Qt::PenStyle style,
                        const QVector<qreal> &dashPattern,
                        Qt::PenCapStyle cap = Qt::SquareCap,
                        Qt::PenJoinStyle join = Qt::BevelJoin)
{
    QPen pen = geometricPen(color, width, style, cap, join);
    if (!dashPattern.isEmpty()) {
        pen.setStyle(Qt::CustomDashLine);
        pen.setDashPattern(dashPattern);
    }
    return pen;
}

quint32 stableHashU32(quint32 value)
{
    value ^= value >> 16;
    value *= 0x7feb352dU;
    value ^= value >> 15;
    value *= 0x846ca68bU;
    value ^= value >> 16;
    return value;
}

qreal stableRandomSigned(quint32 seed, int major, int minor, quint32 salt = 0U)
{
    quint32 value = seed ^ (static_cast<quint32>(major) * 0x9e3779b9U) ^ (static_cast<quint32>(minor) * 0x85ebca6bU) ^ salt;
    value = stableHashU32(value);
    const qreal normalized = static_cast<qreal>(value) / static_cast<qreal>(std::numeric_limits<quint32>::max());
    return (normalized * 2.0) - 1.0;
}

class MapZoomAwareAreaPatternItem final : public QGraphicsPathItem
{
public:
    MapZoomAwareAreaPatternItem(const QPainterPath &path,
                                const MapEditorAreaFillPatternStyle &pattern,
                                const QColor &fallbackColor,
                                int fallbackSeed,
                                QGraphicsItem *parent = nullptr)
        : QGraphicsPathItem(path, parent)
        , pattern_(pattern)
        , fallbackColor_(fallbackColor)
        , fallbackSeed_(fallbackSeed)
    {
        setPen(Qt::NoPen);
        setBrush(Qt::NoBrush);
    }

protected:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override
    {
        Q_UNUSED(widget);
        if (painter == nullptr) {
            return;
        }

        const qreal lod = option != nullptr
            ? QStyleOptionGraphicsItem::levelOfDetailFromTransform(painter->worldTransform())
            : 1.0;

        if (lod < 0.16) {
            return;
        }

        painter->save();
        painter->setClipPath(path(), Qt::IntersectClip);
        painter->setBrush(Qt::NoBrush);
        painter->setRenderHint(QPainter::Antialiasing, true);

        const qreal safeLod = qBound<qreal>(0.12, lod, 16.0);
        const qreal lodZoomIn = qMax<qreal>(1.0, safeLod);
        const qreal densityRelax = std::pow(lodZoomIn, 0.35);
        qreal screenSpacing = qBound<qreal>(4.0, pattern_.spacing, 64.0);
        if (pattern_.kind == MapEditorFillPatternKind::CrossHatch) {
            screenSpacing *= 1.25;
        }
        screenSpacing *= densityRelax;
        const qreal sceneSpacing = qBound<qreal>(0.6, screenSpacing / safeLod, 320.0);
        const qreal sceneStrokeWidth = qBound<qreal>(0.16, (pattern_.strokeWidth / safeLod) / densityRelax, 12.0);
        const qreal sceneRadius = qBound<qreal>(0.3, pattern_.radius / safeLod, 32.0);
        const qreal sceneOffsetJitter = qBound<qreal>(0.0, pattern_.offsetJitter / safeLod, 64.0);
        const int seedValue = pattern_.seed.value_or(fallbackSeed_);
        const quint32 seed = static_cast<quint32>(seedValue == 0 ? 1 : seedValue);
        const QRectF bounds = path().boundingRect();

        auto drawHatch = [&](qreal baseAngle, int orientationIndex) {
            QColor strokeColor = pattern_.strokeColor.value_or(fallbackColor_);
            const qreal baseAlpha = strokeColor.alphaF();
            const qreal alphaByZoom = 1.0 / std::pow(lodZoomIn, 0.22);
            const qreal crossTone = (pattern_.kind == MapEditorFillPatternKind::CrossHatch) ? 0.72 : 0.90;
            strokeColor.setAlphaF(qBound(0.08, baseAlpha * alphaByZoom * crossTone, 1.0));
            QPen pen(strokeColor,
                     sceneStrokeWidth,
                     pattern_.strokeStyle,
                     Qt::SquareCap,
                     Qt::MiterJoin);
            if (!pattern_.dashPattern.isEmpty()) {
                pen.setStyle(Qt::CustomDashLine);
                pen.setDashPattern(pattern_.dashPattern);
            }
            painter->setPen(pen);

            constexpr qreal kPi = 3.14159265358979323846;
            const qreal maxExtent = std::hypot(bounds.width(), bounds.height()) * 1.9 + sceneSpacing;
            const QPointF center = bounds.center();
            const int bandCount = qMax(2, static_cast<int>(std::ceil(maxExtent / sceneSpacing)));
            for (int band = -bandCount; band <= bandCount; ++band) {
                const qreal jitterOffset = sceneOffsetJitter <= 0.0
                    ? 0.0
                    : stableRandomSigned(seed, orientationIndex, band, 0x20411U) * sceneOffsetJitter;
                const qreal jitterAngle = pattern_.angleJitter <= 0.0
                    ? 0.0
                    : stableRandomSigned(seed, orientationIndex, band, 0x8b2f1U) * pattern_.angleJitter;
                const qreal radians = (baseAngle + jitterAngle) * (kPi / 180.0);
                const QPointF direction(std::cos(radians), std::sin(radians));
                const QPointF normal(-direction.y(), direction.x());
                const QPointF shiftedCenter = center + normal * ((static_cast<qreal>(band) * sceneSpacing) + jitterOffset);
                painter->drawLine(shiftedCenter - (direction * maxExtent), shiftedCenter + (direction * maxExtent));
            }
        };

        if (pattern_.kind == MapEditorFillPatternKind::Hatch || pattern_.kind == MapEditorFillPatternKind::CrossHatch) {
            drawHatch(pattern_.angle, 0);
            if (pattern_.kind == MapEditorFillPatternKind::CrossHatch) {
                drawHatch(pattern_.angle + 90.0, 1);
            }
        } else if (pattern_.kind == MapEditorFillPatternKind::Dots) {
            QColor dotColor = pattern_.dotColor.value_or(pattern_.strokeColor.value_or(fallbackColor_));
            const qreal baseAlpha = dotColor.alphaF();
            const qreal alphaByZoom = 1.0 / std::pow(lodZoomIn, 0.20);
            dotColor.setAlphaF(qBound(0.08, baseAlpha * alphaByZoom * 0.88, 1.0));
            painter->setPen(Qt::NoPen);
            painter->setBrush(dotColor);

            const qreal startX = std::floor(bounds.left() / sceneSpacing) * sceneSpacing;
            const qreal startY = std::floor(bounds.top() / sceneSpacing) * sceneSpacing;
            const int cols = qBound(1, static_cast<int>(std::ceil(bounds.width() / sceneSpacing)) + 3, 1200);
            const int rows = qBound(1, static_cast<int>(std::ceil(bounds.height() / sceneSpacing)) + 3, 1200);

            for (int row = 0; row < rows; ++row) {
                for (int col = 0; col < cols; ++col) {
                    const int index = row * 4096 + col;
                    const qreal jitterX = sceneOffsetJitter <= 0.0
                        ? 0.0
                        : stableRandomSigned(seed, 2, index, 0x14f21U) * sceneOffsetJitter;
                    const qreal jitterY = sceneOffsetJitter <= 0.0
                        ? 0.0
                        : stableRandomSigned(seed, 3, index, 0x6a223U) * sceneOffsetJitter;
                    const QPointF dotCenter(startX + (static_cast<qreal>(col) * sceneSpacing) + jitterX,
                                            startY + (static_cast<qreal>(row) * sceneSpacing) + jitterY);
                    painter->drawEllipse(dotCenter, sceneRadius, sceneRadius);
                }
            }
        }

        painter->restore();
    }

private:
    MapEditorAreaFillPatternStyle pattern_;
    QColor fallbackColor_;
    int fallbackSeed_ = 1;
};

MapCanvasTheme mapCanvasThemeForScene(const QGraphicsScene *scene)
{
    Q_UNUSED(scene);

    const QPalette appPalette = QGuiApplication::palette();
    const QColor appWindow = appPalette.color(QPalette::Window);
    const QColor appBase = appPalette.color(QPalette::Base);
    const qreal appReferenceLightness = appWindow.isValid()
        ? appWindow.lightnessF()
        : (appBase.isValid() ? appBase.lightnessF() : 1.0);
    const bool appLooksLight = appReferenceLightness > 0.58;
    const bool appLooksDark = appReferenceLightness < 0.42;

    bool lightMode = appLooksLight;
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    if (QGuiApplication::styleHints() != nullptr) {
        const Qt::ColorScheme scheme = QGuiApplication::styleHints()->colorScheme();
        if (scheme == Qt::ColorScheme::Dark) {
            lightMode = false;
        } else if (scheme == Qt::ColorScheme::Light) {
            lightMode = true;
        }
    }
#endif

    // On some platform/style combinations the color-scheme hint can disagree
    // with the effective application palette; prefer the palette in that case.
    if ((lightMode && appLooksDark) || (!lightMode && appLooksLight)) {
        lightMode = appLooksLight;
    }

    MapCanvasTheme theme;
    theme.lightMode = lightMode;
    if (lightMode) {
        theme.canvasBorder = QColor(QStringLiteral("#bec9d8"));
        theme.canvasFill = QColor(QStringLiteral("#f4f8fd"));
        theme.gridLine = QColor(124, 143, 167, 136);
        theme.geometryStroke = QColor(QStringLiteral("#1d2837"));
        theme.areaFill = QColor(48, 73, 105, 28);
        theme.labelText = QColor(QStringLiteral("#344a67"));
        theme.mutedText = QColor(QStringLiteral("#556b84"));
        theme.pointHandleStroke = QColor(18, 26, 37, 220);
        theme.pointHandleFill = QColor(24, 30, 42, 190);
        theme.controlConnector = QColor(52, 110, 186, 190);
        theme.controlHandleStroke = QColor(20, 73, 148, 230);
        theme.controlHandleFill = QColor(96, 176, 248, 220);
        theme.stationMarker = QColor(QStringLiteral("#cf472e"));
        return theme;
    }

    theme.canvasBorder = QColor(QStringLiteral("#596477"));
    theme.canvasFill = QColor(QStringLiteral("#232833"));
    theme.gridLine = QColor(240, 246, 255, 198);
    theme.geometryStroke = QColor(QStringLiteral("#e1e9f5"));
    theme.areaFill = QColor(220, 227, 238, 24);
    theme.labelText = QColor(QStringLiteral("#e6eefb"));
    theme.mutedText = QColor(QStringLiteral("#a6b4c8"));
    theme.pointHandleStroke = QColor(228, 236, 246, 220);
    theme.pointHandleFill = QColor(206, 219, 235, 190);
    theme.controlConnector = QColor(118, 178, 242, 190);
    theme.controlHandleStroke = QColor(76, 150, 229, 230);
    theme.controlHandleFill = QColor(130, 201, 255, 220);
    theme.stationMarker = QColor(QStringLiteral("#ff6a56"));
    return theme;
}

bool tokenLooksNumeric(const QString &token)
{
    if (token.isEmpty()) {
        return false;
    }

    static const QRegularExpression numericPattern(
        QStringLiteral(R"(^[+-]?(?:(?:\d+(?:\.\d*)?)|(?:\.\d+))(?:[eE][+-]?\d+)?$)"));
    return numericPattern.match(token).hasMatch();
}

std::optional<bool> parseToggleToken(const QString &token)
{
    const QString normalized = token.trimmed().toLower();
    if (normalized == QStringLiteral("on")
        || normalized == QStringLiteral("yes")
        || normalized == QStringLiteral("true")
        || normalized == QStringLiteral("1")) {
        return true;
    }
    if (normalized == QStringLiteral("off")
        || normalized == QStringLiteral("no")
        || normalized == QStringLiteral("false")
        || normalized == QStringLiteral("0")) {
        return false;
    }

    return std::nullopt;
}

std::optional<bool> lineOptionToggleValue(const TherionParsedLine &parsedLine, const QString &optionName)
{
    std::optional<bool> value;
    if (parsedLine.tokens.size() < 2 || optionName.isEmpty()) {
        return value;
    }

    const QString normalizedOption = optionName.toLower();
    for (int index = 1; index < parsedLine.tokens.size(); ++index) {
        const QString token = parsedLine.tokens.at(index).trimmed().toLower();
        if (token != normalizedOption) {
            continue;
        }

        if (index + 1 >= parsedLine.tokens.size()) {
            value = true;
            continue;
        }

        const QString nextToken = parsedLine.tokens.at(index + 1).trimmed();
        if (nextToken.startsWith(QLatin1Char('-')) && !tokenLooksNumeric(nextToken)) {
            value = true;
            continue;
        }

        value = parseToggleToken(nextToken).value_or(true);
    }

    return value;
}

QVector<QPointF> coordinatePointsFromLine(const TherionParsedLine &parsedLine, int startTokenIndex)
{
    QVector<QPointF> points;
    QVector<int> numericIndices;
    numericIndices.reserve(parsedLine.tokens.size());

    const int firstTokenIndex = qMax(0, startTokenIndex);
    if (firstTokenIndex == 0) {
        int firstNonQuotedIndex = -1;
        for (int index = firstTokenIndex; index < parsedLine.tokens.size(); ++index) {
            if (index < parsedLine.tokenSpans.size()
                && parsedLine.tokenSpans.at(index).type == TherionTokenType::QuotedString) {
                continue;
            }
            firstNonQuotedIndex = index;
            break;
        }

        if (firstNonQuotedIndex >= 0 && !tokenLooksNumeric(parsedLine.tokens.at(firstNonQuotedIndex))) {
            return points;
        }
    }

    if (firstTokenIndex < parsedLine.tokens.size()) {
        const QString firstToken = parsedLine.tokens.at(firstTokenIndex);
        if (firstTokenIndex == 0
            && firstToken.startsWith(QLatin1Char('-'))
            && !tokenLooksNumeric(firstToken)) {
            return points;
        }
    }

    bool sawCoordinateToken = false;
    bool skipOptionValueToken = false;
    for (int index = firstTokenIndex; index < parsedLine.tokens.size(); ++index) {
        if (skipOptionValueToken && !sawCoordinateToken) {
            skipOptionValueToken = false;
            continue;
        }

        const QString token = parsedLine.tokens.at(index);
        if (!sawCoordinateToken
            && firstTokenIndex > 0
            && token.startsWith(QLatin1Char('-'))
            && !tokenLooksNumeric(token)) {
            if (index + 1 < parsedLine.tokens.size()) {
                const QString nextToken = parsedLine.tokens.at(index + 1);
                if (!nextToken.startsWith(QLatin1Char('-')) || tokenLooksNumeric(nextToken)) {
                    skipOptionValueToken = true;
                }
            }
            continue;
        }

        const bool numeric = tokenLooksNumeric(token);
        if (!numeric) {
            if (sawCoordinateToken) {
                break;
            }
            continue;
        }

        if (index < parsedLine.tokenSpans.size()
            && parsedLine.tokenSpans.at(index).type == TherionTokenType::QuotedString) {
            continue;
        }

        numericIndices.append(index);
        sawCoordinateToken = true;
    }

    for (int index = 0; index + 1 < numericIndices.size(); index += 2) {
        bool okX = false;
        bool okY = false;
        const qreal x = parsedLine.tokens.at(numericIndices.at(index)).toDouble(&okX);
        const qreal y = parsedLine.tokens.at(numericIndices.at(index + 1)).toDouble(&okY);
        if (okX && okY) {
            points.append(QPointF(x, y));
        }
    }

    return points;
}

struct SourceCoordinatePoint
{
    QPointF point;
    int sourceVertexIndex = -1;
};

QVector<SourceCoordinatePoint> sourceCoordinatePointsFromLine(const TherionParsedLine &parsedLine,
                                                              int startTokenIndex,
                                                              int *nextSourceVertexIndex)
{
    QVector<SourceCoordinatePoint> points;
    if (nextSourceVertexIndex == nullptr) {
        return points;
    }

    const QVector<QPointF> rowPoints = coordinatePointsFromLine(parsedLine, startTokenIndex);
    points.reserve(rowPoints.size());
    for (const QPointF &point : rowPoints) {
        SourceCoordinatePoint coordinatePoint;
        coordinatePoint.point = point;
        coordinatePoint.sourceVertexIndex = *nextSourceVertexIndex;
        ++(*nextSourceVertexIndex);
        points.append(coordinatePoint);
    }

    return points;
}

void appendLineDataPoints(MapGeometryFeature *feature, const QVector<SourceCoordinatePoint> &linePoints)
{
    if (feature == nullptr || linePoints.isEmpty()) {
        return;
    }

    int pointIndex = 0;
    if (feature->lineVertices.isEmpty()) {
        MapGeometryFeature::TH2LineVertex firstVertex;
        firstVertex.anchor = linePoints.first().point;
        firstVertex.anchorSourceVertexIndex = linePoints.first().sourceVertexIndex;
        feature->lineVertices.append(firstVertex);
        feature->vertices.append(firstVertex.anchor);
        pointIndex = 1;
    }

    while (pointIndex < linePoints.size()) {
        const int remaining = linePoints.size() - pointIndex;
        if (remaining == 1) {
            const SourceCoordinatePoint anchorPoint = linePoints.at(pointIndex);
            MapGeometryFeature::TH2LineVertex nextVertex;
            nextVertex.anchor = anchorPoint.point;
            nextVertex.anchorSourceVertexIndex = anchorPoint.sourceVertexIndex;
            feature->lineVertices.append(nextVertex);
            feature->vertices.append(nextVertex.anchor);

            MapGeometryFeature::LineSegment segment;
            segment.type = MapGeometryFeature::LineSegment::Type::Linear;
            segment.endVertexIndex = feature->lineVertices.size() - 1;
            feature->lineSegments.append(segment);
            ++pointIndex;
            continue;
        }

        if (remaining < 3) {
            const SourceCoordinatePoint anchorPoint = linePoints.at(pointIndex);
            MapGeometryFeature::TH2LineVertex nextVertex;
            nextVertex.anchor = anchorPoint.point;
            nextVertex.anchorSourceVertexIndex = anchorPoint.sourceVertexIndex;
            feature->lineVertices.append(nextVertex);
            feature->vertices.append(nextVertex.anchor);

            MapGeometryFeature::LineSegment segment;
            segment.type = MapGeometryFeature::LineSegment::Type::Linear;
            segment.endVertexIndex = feature->lineVertices.size() - 1;
            feature->lineSegments.append(segment);
            ++pointIndex;
            continue;
        }

        const SourceCoordinatePoint outControlPoint = linePoints.at(pointIndex);
        const SourceCoordinatePoint inControlPoint = linePoints.at(pointIndex + 1);
        const SourceCoordinatePoint anchorPoint = linePoints.at(pointIndex + 2);

        const bool closingToFirstAnchor = feature->closed
            && feature->lineVertices.size() >= 2
            && pointIndex + 3 == linePoints.size()
            && !mapSourcePointsDiffer(anchorPoint.point, feature->lineVertices.first().anchor);
        if (closingToFirstAnchor) {
            MapGeometryFeature::TH2LineVertex &previousVertex = feature->lineVertices.last();
            previousVertex.outgoingSourceVertexIndex = outControlPoint.sourceVertexIndex;
            if (mapSourcePointsDiffer(outControlPoint.point, previousVertex.anchor)) {
                previousVertex.outgoingControl = outControlPoint.point;
            } else {
                previousVertex.outgoingControl.reset();
            }
            const bool previousHasOutgoingControl = previousVertex.outgoingControl.has_value();
            const int previousOutgoingSourceIndex = previousVertex.outgoingSourceVertexIndex;

            MapGeometryFeature::TH2LineVertex &firstVertex = feature->lineVertices.first();
            firstVertex.incomingSourceVertexIndex = inControlPoint.sourceVertexIndex;
            if (mapSourcePointsDiffer(inControlPoint.point, firstVertex.anchor)) {
                firstVertex.incomingControl = inControlPoint.point;
            } else {
                firstVertex.incomingControl.reset();
            }

            MapGeometryFeature::LineSegment segment;
            segment.type = (previousHasOutgoingControl || firstVertex.incomingControl.has_value())
                ? MapGeometryFeature::LineSegment::Type::Cubic
                : MapGeometryFeature::LineSegment::Type::Linear;
            segment.endVertexIndex = 0;
            segment.control1VertexIndex = previousOutgoingSourceIndex;
            segment.control2VertexIndex = firstVertex.incomingSourceVertexIndex;
            feature->lineSegments.append(segment);
            pointIndex += 3;
            continue;
        }

        MapGeometryFeature::TH2LineVertex &previousVertex = feature->lineVertices.last();
        previousVertex.outgoingSourceVertexIndex = outControlPoint.sourceVertexIndex;
        if (mapSourcePointsDiffer(outControlPoint.point, previousVertex.anchor)) {
            previousVertex.outgoingControl = outControlPoint.point;
        } else {
            previousVertex.outgoingControl.reset();
        }
        const bool previousHasOutgoingControl = previousVertex.outgoingControl.has_value();
        const int previousOutgoingSourceIndex = previousVertex.outgoingSourceVertexIndex;

        MapGeometryFeature::TH2LineVertex nextVertex;
        nextVertex.anchor = anchorPoint.point;
        nextVertex.anchorSourceVertexIndex = anchorPoint.sourceVertexIndex;
        nextVertex.incomingSourceVertexIndex = inControlPoint.sourceVertexIndex;
        if (mapSourcePointsDiffer(inControlPoint.point, nextVertex.anchor)) {
            nextVertex.incomingControl = inControlPoint.point;
        } else {
            nextVertex.incomingControl.reset();
        }
        feature->lineVertices.append(nextVertex);
        feature->vertices.append(nextVertex.anchor);

        MapGeometryFeature::LineSegment segment;
        segment.type = (previousHasOutgoingControl || nextVertex.incomingControl.has_value())
            ? MapGeometryFeature::LineSegment::Type::Cubic
            : MapGeometryFeature::LineSegment::Type::Linear;
        segment.endVertexIndex = feature->lineVertices.size() - 1;
        segment.control1VertexIndex = previousOutgoingSourceIndex;
        segment.control2VertexIndex = nextVertex.incomingSourceVertexIndex;
        feature->lineSegments.append(segment);
        pointIndex += 3;
    }
}

QPainterPath linePathForFeature(const MapGeometryFeature &feature, const QRectF &sourceBounds, const QRectF &previewBounds)
{
    QPainterPath path;
    if (feature.lineVertices.size() < 2) {
        return path;
    }

    auto toPreview = [&](const QPointF &point) {
        return mapGeometryPointToPreview(point, sourceBounds, previewBounds);
    };

    path.moveTo(toPreview(feature.lineVertices.first().anchor));
    for (int index = 1; index < feature.lineVertices.size(); ++index) {
        const MapGeometryFeature::TH2LineVertex &previousVertex = feature.lineVertices.at(index - 1);
        const MapGeometryFeature::TH2LineVertex &currentVertex = feature.lineVertices.at(index);
        const QPointF cp1 = previousVertex.outgoingControl.value_or(previousVertex.anchor);
        const QPointF cp2 = currentVertex.incomingControl.value_or(currentVertex.anchor);
        const bool hasCurveHandle = previousVertex.outgoingControl.has_value() || currentVertex.incomingControl.has_value();
        if (hasCurveHandle) {
            path.cubicTo(toPreview(cp1), toPreview(cp2), toPreview(currentVertex.anchor));
        } else {
            path.lineTo(toPreview(currentVertex.anchor));
        }
    }

    if (feature.closed && feature.lineVertices.size() >= 3) {
        const MapGeometryFeature::TH2LineVertex &lastVertex = feature.lineVertices.last();
        const MapGeometryFeature::TH2LineVertex &firstVertex = feature.lineVertices.first();
        const QPointF cp1 = lastVertex.outgoingControl.value_or(lastVertex.anchor);
        const QPointF cp2 = firstVertex.incomingControl.value_or(firstVertex.anchor);
        const bool hasCurveHandle = lastVertex.outgoingControl.has_value() || firstVertex.incomingControl.has_value();
        if (hasCurveHandle) {
            path.cubicTo(toPreview(cp1), toPreview(cp2), toPreview(firstVertex.anchor));
        } else {
            path.lineTo(toPreview(firstVertex.anchor));
        }
    }

    return path;
}

std::optional<QLineF> lineDirectionTickLine(const QPointF &firstAnchorPreview,
                                            const std::optional<QPointF> &outgoingControlPreview,
                                            const QPointF &secondAnchorPreview,
                                            bool reversed,
                                            qreal tickLength)
{
    QPointF tangentTarget = secondAnchorPreview;
    if (outgoingControlPreview.has_value()) {
        const QPointF outgoingDelta = outgoingControlPreview.value() - firstAnchorPreview;
        if (std::hypot(outgoingDelta.x(), outgoingDelta.y()) > 0.001) {
            tangentTarget = outgoingControlPreview.value();
        }
    }

    const QPointF tangent = tangentTarget - firstAnchorPreview;
    const qreal tangentLength = std::hypot(tangent.x(), tangent.y());
    if (tangentLength <= 0.001) {
        return std::nullopt;
    }

    QPointF normal(tangent.y(), -tangent.x());
    const qreal normalLength = std::hypot(normal.x(), normal.y());
    if (normalLength <= 0.001) {
        return std::nullopt;
    }

    if (reversed) {
        normal = -normal;
    }
    normal *= tickLength / normalLength;
    return QLineF(firstAnchorPreview, firstAnchorPreview + normal);
}

std::optional<QLineF> lineDirectionTickLineForFeature(const MapGeometryFeature &feature,
                                                      const QRectF &sourceBounds,
                                                      const QRectF &previewBounds,
                                                      qreal tickLength)
{
    if (feature.lineVertices.size() < 2) {
        return std::nullopt;
    }

    const MapGeometryFeature::TH2LineVertex &firstVertex = feature.lineVertices.first();
    const MapGeometryFeature::TH2LineVertex &secondVertex = feature.lineVertices.at(1);
    const QPointF firstAnchorPreview = mapGeometryPointToPreview(firstVertex.anchor, sourceBounds, previewBounds);
    std::optional<QPointF> outgoingControlPreview;
    if (firstVertex.outgoingControl.has_value()) {
        outgoingControlPreview = mapGeometryPointToPreview(firstVertex.outgoingControl.value(), sourceBounds, previewBounds);
    }
    const QPointF secondAnchorPreview = mapGeometryPointToPreview(secondVertex.anchor, sourceBounds, previewBounds);
    return lineDirectionTickLine(firstAnchorPreview,
                                 outgoingControlPreview,
                                 secondAnchorPreview,
                                 feature.reversed,
                                 tickLength);
}

qreal normalizedSceneOrientationDegrees(qreal value)
{
    qreal normalized = std::fmod(value, 360.0);
    if (normalized < 0.0) {
        normalized += 360.0;
    }
    if (normalized >= 360.0) {
        normalized -= 360.0;
    }
    return normalized;
}

bool explicitOrientation(const std::optional<qreal> &orientationDegrees)
{
    return orientationDegrees.has_value();
}

bool linePointOptionTokenMatches(const QString &token, const QStringList &names)
{
    const QString normalized = token.trimmed().toLower();
    for (const QString &name : names) {
        if (normalized == name) {
            return true;
        }
    }
    return false;
}

std::optional<qreal> linePointNumericOptionValue(const TherionParsedLine &parsedLine, const QStringList &names)
{
    std::optional<qreal> value;
    for (int index = 0; index + 1 < parsedLine.tokens.size(); ++index) {
        if (!linePointOptionTokenMatches(parsedLine.tokens.at(index), names)) {
            continue;
        }

        const QString valueToken = parsedLine.tokens.at(index + 1).trimmed();
        if (valueToken.startsWith(QLatin1Char('-')) && !tokenLooksNumeric(valueToken)) {
            continue;
        }

        bool ok = false;
        const qreal parsedValue = valueToken.toDouble(&ok);
        if (ok) {
            value = parsedValue;
        }
    }
    return value;
}

void applyLinePointOptionsFromLine(const TherionParsedLine &parsedLine, MapGeometryFeature *feature)
{
    if (feature == nullptr || feature->lineVertices.isEmpty()) {
        return;
    }

    MapGeometryFeature::TH2LineVertex &vertex = feature->lineVertices.last();
    if (const std::optional<qreal> orientation =
            linePointNumericOptionValue(parsedLine,
                                        QStringList{QStringLiteral("-orientation"),
                                                    QStringLiteral("orientation"),
                                                    QStringLiteral("-orient"),
                                                    QStringLiteral("orient")})) {
        vertex.orientationDegrees = normalizedSceneOrientationDegrees(orientation.value());
    }

    if (feature->label.trimmed().toLower() == QStringLiteral("slope")) {
        if (const std::optional<qreal> leftSize =
                linePointNumericOptionValue(parsedLine,
                                            QStringList{QStringLiteral("-size"),
                                                        QStringLiteral("size"),
                                                        QStringLiteral("-l-size"),
                                                        QStringLiteral("l-size")})) {
            if (leftSize.value() > 0.0) {
                vertex.leftSize = leftSize.value();
            }
        }
    }
}

QString optionValue(const QStringList &tokens, const QString &option)
{
    const QString normalizedOption = option.toLower();
    for (int index = 0; index + 1 < tokens.size(); ++index) {
        if (tokens.at(index).toLower() != normalizedOption) {
            continue;
        }

        const QString candidate = tokens.at(index + 1);
        if (!candidate.startsWith(QLatin1Char('-'))) {
            return candidate;
        }
    }

    return QString();
}

void appendAreaReferenceIdentifiers(const TherionParsedLine &parsedLine,
                                    int startTokenIndex,
                                    QStringList *identifiers)
{
    if (identifiers == nullptr) {
        return;
    }

    for (int index = qMax(0, startTokenIndex); index < parsedLine.tokens.size(); ++index) {
        const QString token = parsedLine.tokens.at(index).trimmed();
        if (token.isEmpty() || tokenLooksNumeric(token) || token.startsWith(QLatin1Char('-'))) {
            continue;
        }
        identifiers->append(token);
    }
}

QVector<QPointF> areaVerticesFromReferencedLines(const QStringList &identifiers,
                                                 const QHash<QString, MapGeometryFeature> &lineFeaturesByIdentifier,
                                                 QVector<MapGeometryFeature::TH2LineVertex> *resolvedLineVertices = nullptr)
{
    QVector<QPointF> vertices;
    if (resolvedLineVertices != nullptr) {
        resolvedLineVertices->clear();
    }

    for (const QString &identifier : identifiers) {
        const QString normalizedIdentifier = identifier.trimmed().toLower();
        if (normalizedIdentifier.isEmpty()) {
            continue;
        }

        const auto featureIt = lineFeaturesByIdentifier.constFind(normalizedIdentifier);
        if (featureIt == lineFeaturesByIdentifier.constEnd()) {
            continue;
        }

        const MapGeometryFeature &lineFeature = featureIt.value();
        if (lineFeature.lineVertices.size() < 2) {
            continue;
        }

        const int existingVertexCount = vertices.size();
        for (int index = 0; index < lineFeature.lineVertices.size(); ++index) {
            const QPointF anchor = lineFeature.lineVertices.at(index).anchor;
            if (!vertices.isEmpty() && vertices.last() == anchor) {
                continue;
            }
            vertices.append(anchor);
            if (resolvedLineVertices != nullptr) {
                resolvedLineVertices->append(lineFeature.lineVertices.at(index));
            }
        }

        if (existingVertexCount > 0
            && !vertices.isEmpty()
            && vertices.first() == vertices.last()) {
            vertices.removeLast();
            if (resolvedLineVertices != nullptr && !resolvedLineVertices->isEmpty()) {
                resolvedLineVertices->removeLast();
            }
        }
    }

    return vertices;
}

} // namespace

QString mapWorkspaceHelpHtml()
{
    return QStringLiteral(
        "<h3>Map Workspace</h3>"
        "<p>The map canvas renders parsed TH2 geometry and supports direct point, line, and area vertex edits where source coordinates are available.</p>"
        "<h4>Toolbar</h4>"
        "<ul>"
        "<li><strong>Select</strong> keeps focus on selecting and moving draft objects.</li>"
        "<li><strong>Point</strong>, <strong>Line</strong>, and <strong>Area</strong> create draft geometry in the scene.</li>"
        "<li><strong>Insert Scrap</strong> appends a new scrap block to the source document and reparses the scene.</li>"
        "<li><strong>Complete Draft</strong> marks the selected draft geometry as finished.</li>"
        "<li><strong>Undo</strong> and <strong>Redo</strong> follow the same command stack as source edits.</li>"
        "<li><strong>Fit</strong> recenters the geometry preview.</li>"
        "<li><strong>Fit + BG</strong> fits the viewport to geometry plus all loaded background image layers.</li>"
        "</ul>"
        "<p>Line-anchor editing shortcuts: <code>Insert</code> (or <code>I</code> on keyboards without Insert) splits the selected segment; <code>Delete</code>/<code>Backspace</code> removes a selected middle anchor; <code>S</code> toggles smooth/corner behavior for the selected line vertex.</p>"
        "<p>Background image layers are managed from the Map sidebar, including layer order, visibility, position, opacity, and gamma.</p>"
        "<p>When present, <code>##XTHERION## xth_me_image_insert</code> metadata is used to auto-load referenced background images.</p>"
        "<p>Drag parsed geometry handles to rewrite source coordinates. Select a draft item to move or toggle it.</p>");
}

QString mapEntryCategoryForLine(const TherionParsedLine &parsedLine)
{
    if (parsedLine.tokens.isEmpty()) {
        return QString();
    }

    const QString directive = parsedLine.tokens.first().toLower();
    const QString secondToken = parsedLine.tokens.value(1).toLower();

    if (directive == QStringLiteral("survey")) {
        return QObject::tr("Survey");
    }
    if (directive == QStringLiteral("map")) {
        return QObject::tr("Map");
    }
    if (directive == QStringLiteral("scrap")) {
        return QObject::tr("Scrap");
    }
    if (directive == QStringLiteral("line")) {
        return QObject::tr("Line");
    }
    if (directive == QStringLiteral("area")) {
        return QObject::tr("Area");
    }
    if (directive == QStringLiteral("station")) {
        return QObject::tr("Station");
    }
    if (directive == QStringLiteral("point") && secondToken == QStringLiteral("station")) {
        return QObject::tr("Station");
    }
    if (directive == QStringLiteral("point")) {
        return QObject::tr("Point");
    }

    return QString();
}

QString mapEntryTitleForLine(const TherionParsedLine &parsedLine)
{
    if (parsedLine.tokens.isEmpty()) {
        return QString();
    }

    const QString directive = parsedLine.tokens.first().toLower();
    if (directive == QStringLiteral("point") && parsedLine.tokens.size() >= 3 && parsedLine.tokens.value(1).toLower() == QStringLiteral("station")) {
        return parsedLine.tokens.value(2);
    }

    if (parsedLine.tokens.size() > 1) {
        return parsedLine.tokens.value(1);
    }

    return parsedLine.tokens.first();
}

QString mapEntrySubtitleForLine(const TherionParsedLine &parsedLine)
{
    if (parsedLine.tokens.size() <= 1) {
        return parsedLine.rawText.trimmed();
    }

    QStringList remainder = parsedLine.tokens.mid(1);
    if (parsedLine.tokens.first().toLower() == QStringLiteral("point") && parsedLine.tokens.value(1).toLower() == QStringLiteral("station") && parsedLine.tokens.size() > 2) {
        remainder = parsedLine.tokens.mid(2);
    }

    QString subtitle = remainder.join(QStringLiteral(" "));
    if (parsedLine.tokens.first().toLower() == QStringLiteral("line")) {
        const bool closed = lineOptionToggleValue(parsedLine, QStringLiteral("-close")).value_or(false);
        const bool reversed = lineOptionToggleValue(parsedLine, QStringLiteral("-reverse")).value_or(false);
        QStringList flags;
        if (closed) {
            flags.append(QObject::tr("closed"));
        }
        if (reversed) {
            flags.append(QObject::tr("reversed"));
        }
        if (!flags.isEmpty()) {
            if (!subtitle.isEmpty()) {
                subtitle += QStringLiteral(" ");
            }
            subtitle += QStringLiteral("[%1]").arg(flags.join(QStringLiteral(", ")));
        }
    }

    return subtitle;
}

QColor mapEntryAccentForCategory(const QString &category)
{
    if (category == QObject::tr("Survey")) {
        return QColor(QStringLiteral("#5aa9ff"));
    }
    if (category == QObject::tr("Map")) {
        return QColor(QStringLiteral("#8f8bff"));
    }
    if (category == QObject::tr("Scrap")) {
        return QColor(QStringLiteral("#4dd6a8"));
    }
    if (category == QObject::tr("Line")) {
        return QColor(QStringLiteral("#ffb15a"));
    }
    if (category == QObject::tr("Area")) {
        return QColor(QStringLiteral("#ff7f8f"));
    }
    if (category == QObject::tr("Station")) {
        return QColor(QStringLiteral("#ffd86b"));
    }
    if (category == QObject::tr("Point")) {
        return QColor(QStringLiteral("#7ed0ff"));
    }

    return QColor(QStringLiteral("#7f8ca3"));
}

QVector<MapSceneEntry> collectMapSceneEntries(const QVector<TherionParsedLine> &parsedLines)
{
    QVector<MapSceneEntry> entries;
    entries.reserve(parsedLines.size());

    for (const TherionParsedLine &parsedLine : parsedLines) {
        const QString category = mapEntryCategoryForLine(parsedLine);
        if (category.isEmpty()) {
            continue;
        }

        MapSceneEntry entry;
        entry.lineNumber = parsedLine.lineNumber;
        entry.category = category;
        entry.title = mapEntryTitleForLine(parsedLine);
        entry.subtitle = mapEntrySubtitleForLine(parsedLine);
        entries.append(entry);
    }

    return entries;
}

void renderMapWorkspaceScene(QGraphicsScene *scene,
                             const QString &documentPath,
                             const QVector<MapSceneEntry> &entries,
                             const QVector<MapGeometryFeature> &geometryFeatures,
                             const std::optional<QRectF> &sourceBoundsOverride,
                             const MapGridOptions &gridOptions,
                             bool showEmptyDocumentGuides,
                             QHash<int, QGraphicsItem *> *mapItemsByLine,
                             const std::function<void(int, const QPointF &, const QPointF &)> &recordCardMove,
                             const std::function<void(int, bool, bool)> &recordCardVisibility,
                             const std::function<void(int, const QPointF &, const QPointF &)> &recordPointGeometryMove,
                             const std::function<void(int, const QString &, int, const QPointF &, const QPointF &)> &recordLineAreaVertexMove,
                             const std::function<void(int, qreal)> &recordPointOrientationHandleChange,
                             const std::function<void(int, int, qreal, qreal)> &recordLinePointLeftHandleChange)
{
    Q_UNUSED(documentPath);
    Q_UNUSED(recordCardMove);
    Q_UNUSED(recordCardVisibility);

    if (scene == nullptr) {
        return;
    }

    if (mapItemsByLine != nullptr) {
        mapItemsByLine->clear();
    }

    const MapCanvasTheme canvasTheme = mapCanvasThemeForScene(scene);
    const QRectF sceneFrame(0, 0, 1200, 900);
    scene->setSceneRect(sceneFrame);
    const QRectF geometryCanvas = sceneFrame.adjusted(24.0, 24.0, -24.0, -24.0);
    if (showEmptyDocumentGuides || !geometryFeatures.isEmpty()) {
        makeMouseTransparent(scene->addRect(geometryCanvas, QPen(canvasTheme.canvasBorder, 1.2), QBrush(canvasTheme.canvasFill)));
    }

    const QRectF previewBounds = geometryCanvas.adjusted(20.0, 20.0, -20.0, -20.0);
    const QRectF sourceBounds = (sourceBoundsOverride.has_value() && sourceBoundsOverride->isValid())
        ? sourceBoundsOverride.value()
        : geometryBoundsForFeatures(geometryFeatures);
    const qreal mapScale = sceneCoordsScaleFactor(sourceBounds, previewBounds);
    const MapEditorObjectStyleCatalog styleCatalog = mapEditorObjectStyleCatalog();
    const qreal vertexRadius = 4.4;
    const qreal gridLineWidth = canvasTheme.lightMode ? 1.0 : 1.1;
    auto markGeometryItem = [](QGraphicsItem *item) {
        if (item != nullptr) {
            item->setData(kMapItemRole, kMapItemGeometryValue);
        }
    };

    if ((showEmptyDocumentGuides || !geometryFeatures.isEmpty()) && gridOptions.visible && sourceBounds.isValid()) {
        const QRectF fittedPreviewBounds = sceneCoordsPreviewBounds(sourceBounds, previewBounds);
        const qreal sourceUnitsPerMeter = qBound(1e-6, gridOptions.sourceUnitsPerMeter, 1e9);
        const qreal spacing = qBound(0.1, gridOptions.spacingMeters * sourceUnitsPerMeter, 1e9);
        constexpr int maxGridLinesPerAxis = 1000;

        const qreal firstX = std::floor(sourceBounds.left() / spacing) * spacing;
        const qreal lastX = std::ceil(sourceBounds.right() / spacing) * spacing;
        int verticalLineCount = 0;
        for (qreal sourceX = firstX; sourceX <= lastX + (spacing * 0.5) && verticalLineCount < maxGridLinesPerAxis; sourceX += spacing, ++verticalLineCount) {
            const QPointF topPoint = mapGeometryPointToPreview(QPointF(sourceX, sourceBounds.top()), sourceBounds, previewBounds);
            const QPointF bottomPoint = mapGeometryPointToPreview(QPointF(sourceX, sourceBounds.bottom()), sourceBounds, previewBounds);
            makeMouseTransparent(scene->addLine(topPoint.x(), fittedPreviewBounds.top(), bottomPoint.x(), fittedPreviewBounds.bottom(), QPen(canvasTheme.gridLine, gridLineWidth, Qt::SolidLine)));
        }

        const qreal firstY = std::floor(sourceBounds.top() / spacing) * spacing;
        const qreal lastY = std::ceil(sourceBounds.bottom() / spacing) * spacing;
        int horizontalLineCount = 0;
        for (qreal sourceY = firstY; sourceY <= lastY + (spacing * 0.5) && horizontalLineCount < maxGridLinesPerAxis; sourceY += spacing, ++horizontalLineCount) {
            const QPointF leftPoint = mapGeometryPointToPreview(QPointF(sourceBounds.left(), sourceY), sourceBounds, previewBounds);
            const QPointF rightPoint = mapGeometryPointToPreview(QPointF(sourceBounds.right(), sourceY), sourceBounds, previewBounds);
            makeMouseTransparent(scene->addLine(fittedPreviewBounds.left(), leftPoint.y(), fittedPreviewBounds.right(), rightPoint.y(), QPen(canvasTheme.gridLine, gridLineWidth, Qt::SolidLine)));
        }
    }

    if (geometryFeatures.isEmpty() && showEmptyDocumentGuides) {
        auto *emptyGeometryItem = makeMouseTransparent(scene->addText(QObject::tr("No parseable point, line, or area geometry was found in this document yet."), QFont(QStringLiteral("Menlo"), 11)));
        emptyGeometryItem->setDefaultTextColor(canvasTheme.mutedText);
        emptyGeometryItem->setPos(previewBounds.left() + 16.0, previewBounds.top() + 16.0);
    } else {
        for (const MapGeometryFeature &feature : geometryFeatures) {
            switch (feature.kind) {
            case MapGeometryFeature::Kind::Point: {
                if (!feature.hasAnchor) {
                    break;
                }

                const MapEditorResolvedPointStyle pointStyle = resolveMapEditorPointStyle(styleCatalog,
                                                                                           feature.stationPoint ? QStringLiteral("station") : feature.label,
                                                                                           feature.subtype);
                const qreal pointRadius = qBound(2.0, pointStyle.radius, 24.0);
                const QColor pointFillColor = pointStyle.fillColor.value_or(canvasTheme.pointHandleFill);
                const QColor pointStrokeColor = pointStyle.strokeColor.value_or(canvasTheme.pointHandleStroke);
                const QPointF previewPoint = mapGeometryPointToPreview(feature.anchor, sourceBounds, previewBounds);
                auto *pointItem = new MapEditablePointItem(feature.lineNumber, feature.anchor, sourceBounds, previewBounds, feature.stationPoint);
                pointItem->setRect(QRectF(-pointRadius, -pointRadius, pointRadius * 2.0, pointRadius * 2.0));
                pointItem->setPen(cosmeticPen(pointStrokeColor, qBound(0.6, pointStyle.outlineWidth, 8.0)));
                pointItem->setBrush(QBrush(pointFillColor));
                pointItem->setMoveCommittedCallback(recordPointGeometryMove);
                scene->addItem(pointItem);
                pointItem->setZValue(3.0);
                markGeometryItem(pointItem);
                pointItem->setData(kMapSceneLineNumberRole, feature.lineNumber);
                if (mapItemsByLine != nullptr && feature.lineNumber > 0) {
                    mapItemsByLine->insert(feature.lineNumber, pointItem);
                }

                if (feature.orientationSupported
                    && explicitOrientation(feature.orientationDegrees)
                    && recordPointOrientationHandleChange) {
                    auto *orientationHandle = new MapPointOrientationHandleItem(feature.lineNumber,
                                                                                previewPoint,
                                                                                feature.orientationDegrees.value(),
                                                                                24.0);
                    orientationHandle->setChangeCommittedCallback(recordPointOrientationHandleChange);
                    scene->addItem(orientationHandle);
                    orientationHandle->setZValue(4.6);
                    markGeometryItem(orientationHandle);
                    orientationHandle->setData(kMapSceneLineNumberRole, feature.lineNumber);
                    orientationHandle->setData(kMapSceneSelectionGatedRole, true);
                    orientationHandle->setData(kMapSceneSelectionSubtypeRole, kMapSceneSelectionSubtypePointOrientationHandle);
                    orientationHandle->setVisible(false);
                }

                if (feature.stationPoint) {
                    QPainterPath markerPath;
                    markerPath.moveTo(previewPoint + QPointF(0.0, -12.0));
                    markerPath.lineTo(previewPoint + QPointF(10.0, 6.0));
                    markerPath.lineTo(previewPoint + QPointF(-10.0, 6.0));
                    markerPath.closeSubpath();

                    auto *triangle = scene->addPath(markerPath, QPen(canvasTheme.stationMarker, 1.2), QBrush(canvasTheme.stationMarker));
                    triangle->setZValue(3.5);
                    markGeometryItem(triangle);
                    makeMouseTransparent(triangle);
                }

                if (feature.stationPoint) {
                    auto *label = makeMouseTransparent(scene->addText(feature.label.isEmpty() ? feature.category : feature.label, QFont(QStringLiteral("Menlo"), 10, QFont::Bold)));
                    label->setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
                    label->setDefaultTextColor(canvasTheme.labelText);
                    label->setPos(previewPoint + QPointF(10.0, -18.0));
                    label->setZValue(4.0);
                }
                break;
            }
            case MapGeometryFeature::Kind::Line: {
                if (feature.lineVertices.size() < 2) {
                    break;
                }

                const MapEditorResolvedLineStyle lineStyle = resolveMapEditorLineStyle(styleCatalog,
                                                                                       feature.label,
                                                                                       feature.subtype);
                const qreal thickLineWidth = qBound(0.8, lineStyle.strokeWidth, 24.0);
                const QPainterPath path = linePathForFeature(feature, sourceBounds, previewBounds);
                QColor geometryStroke = lineStyle.strokeColor.value_or(canvasTheme.geometryStroke);

                auto *lineItem = new MapZoomAwarePathItem(path,
                                                          styledGeometricPen(geometryStroke,
                                                                             thickLineWidth,
                                                                             lineStyle.penStyle,
                                                                             lineStyle.dashPattern,
                                                                             Qt::RoundCap,
                                                                             Qt::RoundJoin));
                scene->addItem(lineItem);
                lineItem->setFlag(QGraphicsItem::ItemIsSelectable, true);
                lineItem->setZValue(2.5);
                markGeometryItem(lineItem);
                lineItem->setData(kMapSceneLineNumberRole, feature.lineNumber);
                const qreal lineDirectionTickLength = qBound(12.0, 18.0 * mapScale, 24.0);
                auto *directionTickItem = new QGraphicsLineItem;
                directionTickItem->setPen(cosmeticPen(QColor(QStringLiteral("#ffda00")),
                                               3.4,
                                                      Qt::SolidLine,
                                                      Qt::RoundCap,
                                                      Qt::RoundJoin));
                directionTickItem->setZValue(4.8);
                markGeometryItem(directionTickItem);
                makeMouseTransparent(directionTickItem);
                directionTickItem->setData(kMapSceneLineNumberRole, feature.lineNumber);
                directionTickItem->setData(kMapSceneSelectionGatedRole, true);
                directionTickItem->setData(kMapSceneSelectionSubtypeRole, kMapSceneSelectionSubtypeLineDetail);
                directionTickItem->setVisible(false);
                if (const std::optional<QLineF> tickLine = lineDirectionTickLineForFeature(feature,
                                                                                           sourceBounds,
                                                                                           previewBounds,
                                                                                           lineDirectionTickLength)) {
                    directionTickItem->setLine(tickLine.value());
                }
                scene->addItem(directionTickItem);
                if (mapItemsByLine != nullptr && feature.lineNumber > 0) {
                    mapItemsByLine->insert(feature.lineNumber, lineItem);
                }

                auto anchorItemsByOrder = std::make_shared<QVector<MapEditableGeometryVertexItem *>>(feature.lineVertices.size(), nullptr);
                auto controlItemsBySourceVertex = std::make_shared<QHash<int, MapEditableGeometryVertexItem *>>();
                auto controlConnectors = std::make_shared<QVector<LineControlConnectorBinding>>();

                for (int vertexIndex = 0; vertexIndex < feature.lineVertices.size(); ++vertexIndex) {
                    const MapGeometryFeature::TH2LineVertex &vertex = feature.lineVertices.at(vertexIndex);
                    auto *vertexItem = new MapEditableGeometryVertexItem(feature.lineNumber,
                                                                         QStringLiteral("line"),
                                                                         vertex.anchorSourceVertexIndex >= 0 ? vertex.anchorSourceVertexIndex : vertexIndex,
                                                                         vertex.anchor,
                                                                         sourceBounds,
                                                                         previewBounds);
                    vertexItem->setRect(QRectF(-vertexRadius, -vertexRadius, vertexRadius * 2.0, vertexRadius * 2.0));
                    QColor vertexFill = feature.accent;
                    vertexFill.setAlpha(185);
                    QColor vertexOutline = feature.accent.darker(220);
                    vertexOutline.setAlpha(220);
                    vertexItem->setPen(cosmeticPen(vertexOutline, 1.0));
                    vertexItem->setBrush(QBrush(vertexFill));
                    vertexItem->setMoveCommittedCallback(recordLineAreaVertexMove);
                    scene->addItem(vertexItem);
                    vertexItem->setZValue(4.0);
                    markGeometryItem(vertexItem);
                    const int anchorSourceVertexIndex = vertex.anchorSourceVertexIndex >= 0 ? vertex.anchorSourceVertexIndex : vertexIndex;
                    vertexItem->setData(kMapSceneLineNumberRole, feature.lineNumber);
                    vertexItem->setData(kMapSceneSelectionGatedRole, true);
                    vertexItem->setData(kMapSceneSelectionSubtypeRole, kMapSceneSelectionSubtypeLineAnchor);
                    vertexItem->setData(kMapSceneOwnerVertexRole, anchorSourceVertexIndex);
                    vertexItem->setVisible(false);
                    anchorItemsByOrder->operator[](vertexIndex) = vertexItem;
                }

                const bool slopeLine = feature.label.trimmed().toLower() == QStringLiteral("slope");
                if (slopeLine && recordLinePointLeftHandleChange) {
                    for (int vertexIndex = 0; vertexIndex < feature.lineVertices.size(); ++vertexIndex) {
                        const MapGeometryFeature::TH2LineVertex &vertex = feature.lineVertices.at(vertexIndex);
                        if (!explicitOrientation(vertex.orientationDegrees)) {
                            continue;
                        }
                        const int anchorSourceVertexIndex = vertex.anchorSourceVertexIndex >= 0
                            ? vertex.anchorSourceVertexIndex
                            : vertexIndex;
                        const QPointF anchorPreview = mapGeometryPointToPreview(vertex.anchor, sourceBounds, previewBounds);
                        const qreal orientationDegrees = vertex.orientationDegrees.value();
                        const qreal leftSize = vertex.leftSize.value_or(40.0);
                        auto *leftHandle = new MapLinePointSizeHandleItem(feature.lineNumber,
                                                                          anchorSourceVertexIndex,
                                                                          anchorPreview,
                                                                          orientationDegrees,
                                                                          leftSize,
                                                                          mapScale);
                        leftHandle->setChangeCommittedCallback(recordLinePointLeftHandleChange);
                        scene->addItem(leftHandle);
                        leftHandle->setZValue(4.7);
                        markGeometryItem(leftHandle);
                        leftHandle->setData(kMapSceneLineNumberRole, feature.lineNumber);
                        leftHandle->setData(kMapSceneSelectionGatedRole, true);
                        leftHandle->setData(kMapSceneSelectionSubtypeRole, kMapSceneSelectionSubtypeLineControlConnector);
                        leftHandle->setData(kMapSceneOwnerVertexRole, anchorSourceVertexIndex);
                        leftHandle->setVisible(false);
                    }
                }

                const qreal controlRadius = 4.0;
                for (int segmentIndex = 1; segmentIndex < feature.lineVertices.size(); ++segmentIndex) {
                    const MapGeometryFeature::TH2LineVertex &previousVertex = feature.lineVertices.at(segmentIndex - 1);
                    const MapGeometryFeature::TH2LineVertex &currentVertex = feature.lineVertices.at(segmentIndex);

                    if (previousVertex.outgoingControl.has_value() && previousVertex.outgoingSourceVertexIndex >= 0) {
                        const QPointF anchorPreview = mapGeometryPointToPreview(previousVertex.anchor, sourceBounds, previewBounds);
                        const QPointF controlPreview = mapGeometryPointToPreview(previousVertex.outgoingControl.value(), sourceBounds, previewBounds);
                        auto *connector = scene->addLine(QLineF(anchorPreview, controlPreview),
                                                         cosmeticPen(canvasTheme.controlConnector, qBound(0.7, 1.0 * mapScale, 1.4), Qt::DashLine, Qt::RoundCap));
                        connector->setZValue(3.2);
                        markGeometryItem(connector);
                        makeMouseTransparent(connector);
                        const int ownerAnchorVertexIndex = previousVertex.anchorSourceVertexIndex >= 0
                            ? previousVertex.anchorSourceVertexIndex
                            : (segmentIndex - 1);
                        connector->setData(kMapSceneLineNumberRole, feature.lineNumber);
                        connector->setData(kMapSceneSelectionGatedRole, true);
                        connector->setData(kMapSceneSelectionSubtypeRole, kMapSceneSelectionSubtypeLineControlConnector);
                        connector->setData(kMapSceneOwnerVertexRole, ownerAnchorVertexIndex);
                        connector->setVisible(false);
                        LineControlConnectorBinding binding;
                        binding.anchorVertexOrder = segmentIndex - 1;
                        binding.controlSourceVertexIndex = previousVertex.outgoingSourceVertexIndex;
                        binding.lineItem = connector;
                        controlConnectors->append(binding);

                        auto *controlItem = new MapEditableGeometryVertexItem(feature.lineNumber,
                                                                              QStringLiteral("line control"),
                                                                              previousVertex.outgoingSourceVertexIndex,
                                                                              previousVertex.outgoingControl.value(),
                                                                              sourceBounds,
                                                                              previewBounds);
                        controlItem->setRect(QRectF(-controlRadius, -controlRadius, controlRadius * 2.0, controlRadius * 2.0));
                        controlItem->setPen(cosmeticPen(canvasTheme.controlHandleStroke, 1.0));
                        controlItem->setBrush(QBrush(canvasTheme.controlHandleFill));
                        controlItem->setMoveCommittedCallback(recordLineAreaVertexMove);
                        scene->addItem(controlItem);
                        controlItem->setZValue(4.2);
                        markGeometryItem(controlItem);
                        controlItem->setData(kMapSceneLineNumberRole, feature.lineNumber);
                        controlItem->setData(kMapSceneSelectionGatedRole, true);
                        controlItem->setData(kMapSceneSelectionSubtypeRole, kMapSceneSelectionSubtypeLineControl);
                        controlItem->setData(kMapSceneOwnerVertexRole, ownerAnchorVertexIndex);
                        controlItem->setVisible(false);
                        controlItemsBySourceVertex->insert(previousVertex.outgoingSourceVertexIndex, controlItem);
                    }

                    if (currentVertex.incomingControl.has_value() && currentVertex.incomingSourceVertexIndex >= 0) {
                        const QPointF anchorPreview = mapGeometryPointToPreview(currentVertex.anchor, sourceBounds, previewBounds);
                        const QPointF controlPreview = mapGeometryPointToPreview(currentVertex.incomingControl.value(), sourceBounds, previewBounds);
                        auto *connector = scene->addLine(QLineF(anchorPreview, controlPreview),
                                                         cosmeticPen(canvasTheme.controlConnector, qBound(0.7, 1.0 * mapScale, 1.4), Qt::DashLine, Qt::RoundCap));
                        connector->setZValue(3.2);
                        markGeometryItem(connector);
                        makeMouseTransparent(connector);
                        const int ownerAnchorVertexIndex = currentVertex.anchorSourceVertexIndex >= 0
                            ? currentVertex.anchorSourceVertexIndex
                            : segmentIndex;
                        connector->setData(kMapSceneLineNumberRole, feature.lineNumber);
                        connector->setData(kMapSceneSelectionGatedRole, true);
                        connector->setData(kMapSceneSelectionSubtypeRole, kMapSceneSelectionSubtypeLineControlConnector);
                        connector->setData(kMapSceneOwnerVertexRole, ownerAnchorVertexIndex);
                        connector->setVisible(false);
                        LineControlConnectorBinding binding;
                        binding.anchorVertexOrder = segmentIndex;
                        binding.controlSourceVertexIndex = currentVertex.incomingSourceVertexIndex;
                        binding.lineItem = connector;
                        controlConnectors->append(binding);

                        auto *controlItem = new MapEditableGeometryVertexItem(feature.lineNumber,
                                                                              QStringLiteral("line control"),
                                                                              currentVertex.incomingSourceVertexIndex,
                                                                              currentVertex.incomingControl.value(),
                                                                              sourceBounds,
                                                                              previewBounds);
                        controlItem->setRect(QRectF(-controlRadius, -controlRadius, controlRadius * 2.0, controlRadius * 2.0));
                        controlItem->setPen(cosmeticPen(canvasTheme.controlHandleStroke, 1.0));
                        controlItem->setBrush(QBrush(canvasTheme.controlHandleFill));
                        controlItem->setMoveCommittedCallback(recordLineAreaVertexMove);
                        scene->addItem(controlItem);
                        controlItem->setZValue(4.2);
                        markGeometryItem(controlItem);
                        controlItem->setData(kMapSceneLineNumberRole, feature.lineNumber);
                        controlItem->setData(kMapSceneSelectionGatedRole, true);
                        controlItem->setData(kMapSceneSelectionSubtypeRole, kMapSceneSelectionSubtypeLineControl);
                        controlItem->setData(kMapSceneOwnerVertexRole, ownerAnchorVertexIndex);
                        controlItem->setVisible(false);
                        controlItemsBySourceVertex->insert(currentVertex.incomingSourceVertexIndex, controlItem);
                    }
                }
                if (feature.closed && feature.lineVertices.size() >= 3) {
                    const MapGeometryFeature::TH2LineVertex &lastVertex = feature.lineVertices.last();
                    const MapGeometryFeature::TH2LineVertex &firstVertex = feature.lineVertices.first();

                    if (lastVertex.outgoingControl.has_value() && lastVertex.outgoingSourceVertexIndex >= 0) {
                        const QPointF anchorPreview = mapGeometryPointToPreview(lastVertex.anchor, sourceBounds, previewBounds);
                        const QPointF controlPreview = mapGeometryPointToPreview(lastVertex.outgoingControl.value(), sourceBounds, previewBounds);
                        auto *connector = scene->addLine(QLineF(anchorPreview, controlPreview),
                                                         cosmeticPen(canvasTheme.controlConnector, qBound(0.7, 1.0 * mapScale, 1.4), Qt::DashLine, Qt::RoundCap));
                        connector->setZValue(3.2);
                        markGeometryItem(connector);
                        makeMouseTransparent(connector);
                        const int ownerAnchorVertexIndex = lastVertex.anchorSourceVertexIndex >= 0
                            ? lastVertex.anchorSourceVertexIndex
                            : (feature.lineVertices.size() - 1);
                        connector->setData(kMapSceneLineNumberRole, feature.lineNumber);
                        connector->setData(kMapSceneSelectionGatedRole, true);
                        connector->setData(kMapSceneSelectionSubtypeRole, kMapSceneSelectionSubtypeLineControlConnector);
                        connector->setData(kMapSceneOwnerVertexRole, ownerAnchorVertexIndex);
                        connector->setVisible(false);
                        LineControlConnectorBinding binding;
                        binding.anchorVertexOrder = feature.lineVertices.size() - 1;
                        binding.controlSourceVertexIndex = lastVertex.outgoingSourceVertexIndex;
                        binding.lineItem = connector;
                        controlConnectors->append(binding);

                        auto *controlItem = new MapEditableGeometryVertexItem(feature.lineNumber,
                                                                              QStringLiteral("line control"),
                                                                              lastVertex.outgoingSourceVertexIndex,
                                                                              lastVertex.outgoingControl.value(),
                                                                              sourceBounds,
                                                                              previewBounds);
                        controlItem->setRect(QRectF(-controlRadius, -controlRadius, controlRadius * 2.0, controlRadius * 2.0));
                        controlItem->setPen(cosmeticPen(canvasTheme.controlHandleStroke, 1.0));
                        controlItem->setBrush(QBrush(canvasTheme.controlHandleFill));
                        controlItem->setMoveCommittedCallback(recordLineAreaVertexMove);
                        scene->addItem(controlItem);
                        controlItem->setZValue(4.2);
                        markGeometryItem(controlItem);
                        controlItem->setData(kMapSceneLineNumberRole, feature.lineNumber);
                        controlItem->setData(kMapSceneSelectionGatedRole, true);
                        controlItem->setData(kMapSceneSelectionSubtypeRole, kMapSceneSelectionSubtypeLineControl);
                        controlItem->setData(kMapSceneOwnerVertexRole, ownerAnchorVertexIndex);
                        controlItem->setVisible(false);
                        controlItemsBySourceVertex->insert(lastVertex.outgoingSourceVertexIndex, controlItem);
                    }

                    if (firstVertex.incomingControl.has_value() && firstVertex.incomingSourceVertexIndex >= 0) {
                        const QPointF anchorPreview = mapGeometryPointToPreview(firstVertex.anchor, sourceBounds, previewBounds);
                        const QPointF controlPreview = mapGeometryPointToPreview(firstVertex.incomingControl.value(), sourceBounds, previewBounds);
                        auto *connector = scene->addLine(QLineF(anchorPreview, controlPreview),
                                                         cosmeticPen(canvasTheme.controlConnector, qBound(0.7, 1.0 * mapScale, 1.4), Qt::DashLine, Qt::RoundCap));
                        connector->setZValue(3.2);
                        markGeometryItem(connector);
                        makeMouseTransparent(connector);
                        const int ownerAnchorVertexIndex = firstVertex.anchorSourceVertexIndex >= 0
                            ? firstVertex.anchorSourceVertexIndex
                            : 0;
                        connector->setData(kMapSceneLineNumberRole, feature.lineNumber);
                        connector->setData(kMapSceneSelectionGatedRole, true);
                        connector->setData(kMapSceneSelectionSubtypeRole, kMapSceneSelectionSubtypeLineControlConnector);
                        connector->setData(kMapSceneOwnerVertexRole, ownerAnchorVertexIndex);
                        connector->setVisible(false);
                        LineControlConnectorBinding binding;
                        binding.anchorVertexOrder = 0;
                        binding.controlSourceVertexIndex = firstVertex.incomingSourceVertexIndex;
                        binding.lineItem = connector;
                        controlConnectors->append(binding);

                        auto *controlItem = new MapEditableGeometryVertexItem(feature.lineNumber,
                                                                              QStringLiteral("line control"),
                                                                              firstVertex.incomingSourceVertexIndex,
                                                                              firstVertex.incomingControl.value(),
                                                                              sourceBounds,
                                                                              previewBounds);
                        controlItem->setRect(QRectF(-controlRadius, -controlRadius, controlRadius * 2.0, controlRadius * 2.0));
                        controlItem->setPen(cosmeticPen(canvasTheme.controlHandleStroke, 1.0));
                        controlItem->setBrush(QBrush(canvasTheme.controlHandleFill));
                        controlItem->setMoveCommittedCallback(recordLineAreaVertexMove);
                        scene->addItem(controlItem);
                        controlItem->setZValue(4.2);
                        markGeometryItem(controlItem);
                        controlItem->setData(kMapSceneLineNumberRole, feature.lineNumber);
                        controlItem->setData(kMapSceneSelectionGatedRole, true);
                        controlItem->setData(kMapSceneSelectionSubtypeRole, kMapSceneSelectionSubtypeLineControl);
                        controlItem->setData(kMapSceneOwnerVertexRole, ownerAnchorVertexIndex);
                        controlItem->setVisible(false);
                        controlItemsBySourceVertex->insert(firstVertex.incomingSourceVertexIndex, controlItem);
                    }
                }

                const auto previewToSource = [sourceBounds, previewBounds](const QPointF &previewPoint) {
                    return sceneCoordsPreviewToSource(previewPoint, sourceBounds, previewBounds);
                };
                const auto sourceToPreview = [sourceBounds, previewBounds](const QPointF &sourcePoint) {
                    return mapGeometryPointToPreview(sourcePoint, sourceBounds, previewBounds);
                };
                auto couplingGuard = std::make_shared<bool>(false);
                const auto updateInteractiveLinePreview = [lineItem,
                                                           directionTickItem,
                                                           lineDirectionTickLength,
                                                           feature,
                                                           sourceBounds,
                                                           previewBounds,
                                                           anchorItemsByOrder,
                                                           controlItemsBySourceVertex,
                                                           controlConnectors]() {
                    if (lineItem == nullptr || anchorItemsByOrder == nullptr) {
                        return;
                    }
                    if (anchorItemsByOrder->size() < 2) {
                        return;
                    }

                    auto anchorPreviewAt = [&](int index) -> QPointF {
                        if (index >= 0 && index < anchorItemsByOrder->size()) {
                            if (MapEditableGeometryVertexItem *item = anchorItemsByOrder->at(index)) {
                                return item->pos();
                            }
                        }
                        if (index >= 0 && index < feature.lineVertices.size()) {
                            return mapGeometryPointToPreview(feature.lineVertices.at(index).anchor, sourceBounds, previewBounds);
                        }
                        return QPointF();
                    };

                    QPainterPath interactivePath;
                    interactivePath.moveTo(anchorPreviewAt(0));
                    for (int index = 1; index < anchorItemsByOrder->size(); ++index) {
                        const MapGeometryFeature::TH2LineVertex &previousVertex = feature.lineVertices.at(index - 1);
                        const MapGeometryFeature::TH2LineVertex &currentVertex = feature.lineVertices.at(index);
                        const QPointF previousAnchor = anchorPreviewAt(index - 1);
                        const QPointF currentAnchor = anchorPreviewAt(index);

                        QPointF cp1 = previousAnchor;
                        QPointF cp2 = currentAnchor;
                        if (previousVertex.outgoingSourceVertexIndex >= 0) {
                            if (MapEditableGeometryVertexItem *control = controlItemsBySourceVertex->value(previousVertex.outgoingSourceVertexIndex, nullptr)) {
                                cp1 = control->pos();
                            }
                        }
                        if (currentVertex.incomingSourceVertexIndex >= 0) {
                            if (MapEditableGeometryVertexItem *control = controlItemsBySourceVertex->value(currentVertex.incomingSourceVertexIndex, nullptr)) {
                                cp2 = control->pos();
                            }
                        }

                        const bool hasCurveHandle = previousVertex.outgoingControl.has_value() || currentVertex.incomingControl.has_value();
                        if (hasCurveHandle) {
                            interactivePath.cubicTo(cp1, cp2, currentAnchor);
                        } else {
                            interactivePath.lineTo(currentAnchor);
                        }
                    }

                    if (feature.closed && anchorItemsByOrder->size() >= 3) {
                        const MapGeometryFeature::TH2LineVertex &lastVertex = feature.lineVertices.last();
                        const MapGeometryFeature::TH2LineVertex &firstVertex = feature.lineVertices.first();
                        const QPointF lastAnchor = anchorPreviewAt(anchorItemsByOrder->size() - 1);
                        const QPointF firstAnchor = anchorPreviewAt(0);

                        QPointF cp1 = lastAnchor;
                        QPointF cp2 = firstAnchor;
                        if (lastVertex.outgoingSourceVertexIndex >= 0) {
                            if (MapEditableGeometryVertexItem *control = controlItemsBySourceVertex->value(lastVertex.outgoingSourceVertexIndex, nullptr)) {
                                cp1 = control->pos();
                            }
                        }
                        if (firstVertex.incomingSourceVertexIndex >= 0) {
                            if (MapEditableGeometryVertexItem *control = controlItemsBySourceVertex->value(firstVertex.incomingSourceVertexIndex, nullptr)) {
                                cp2 = control->pos();
                            }
                        }

                        const bool hasCurveHandle = lastVertex.outgoingControl.has_value() || firstVertex.incomingControl.has_value();
                        if (hasCurveHandle) {
                            interactivePath.cubicTo(cp1, cp2, firstAnchor);
                        } else {
                            interactivePath.lineTo(firstAnchor);
                        }
                    }

                    lineItem->setPath(interactivePath);
                    std::optional<QPointF> outgoingControlPreview;
                    const MapGeometryFeature::TH2LineVertex &firstVertex = feature.lineVertices.first();
                    if (firstVertex.outgoingSourceVertexIndex >= 0 && controlItemsBySourceVertex != nullptr) {
                        if (MapEditableGeometryVertexItem *control = controlItemsBySourceVertex->value(firstVertex.outgoingSourceVertexIndex, nullptr)) {
                            outgoingControlPreview = control->pos();
                        }
                    } else if (firstVertex.outgoingControl.has_value()) {
                        outgoingControlPreview = mapGeometryPointToPreview(firstVertex.outgoingControl.value(), sourceBounds, previewBounds);
                    }
                    if (directionTickItem != nullptr) {
                        if (const std::optional<QLineF> tickLine = lineDirectionTickLine(anchorPreviewAt(0),
                                                                                        outgoingControlPreview,
                                                                                        anchorPreviewAt(1),
                                                                                        feature.reversed,
                                                                                        lineDirectionTickLength)) {
                            directionTickItem->setLine(tickLine.value());
                        } else {
                            directionTickItem->setLine(QLineF(anchorPreviewAt(0), anchorPreviewAt(0)));
                        }
                    }

                    if (controlConnectors != nullptr) {
                        for (const LineControlConnectorBinding &binding : *controlConnectors) {
                            if (binding.lineItem == nullptr) {
                                continue;
                            }
                            const QPointF anchorPoint = anchorPreviewAt(binding.anchorVertexOrder);
                            QPointF controlPoint = anchorPoint;
                            if (controlItemsBySourceVertex != nullptr) {
                                if (MapEditableGeometryVertexItem *control = controlItemsBySourceVertex->value(binding.controlSourceVertexIndex, nullptr)) {
                                    controlPoint = control->pos();
                                }
                            }
                            binding.lineItem->setLine(QLineF(anchorPoint, controlPoint));
                        }
                    }
                };

                const auto previewLineMove = [feature,
                                              controlItemsBySourceVertex,
                                              previewToSource,
                                              sourceToPreview,
                                              couplingGuard,
                                              updateInteractiveLinePreview](MapEditableGeometryVertexItem *movedItem,
                                                                            const QPointF &previousSourcePoint,
                                                                            const QPointF &newSourcePoint,
                                                                            bool dragActive) {
                    if (movedItem == nullptr) {
                        updateInteractiveLinePreview();
                        return;
                    }
                    if (*couplingGuard || !dragActive) {
                        updateInteractiveLinePreview();
                        return;
                    }
                    if (movedItem->geometryKind() != QStringLiteral("line")
                        && movedItem->geometryKind() != QStringLiteral("line control")) {
                        updateInteractiveLinePreview();
                        return;
                    }

                    const int movedSourceVertexIndex = movedItem->vertexIndex();
                    if (movedSourceVertexIndex < 0 || controlItemsBySourceVertex == nullptr) {
                        updateInteractiveLinePreview();
                        return;
                    }

                    QHash<int, QPointF> currentControlPoints;
                    currentControlPoints.reserve(controlItemsBySourceVertex->size());
                    for (auto it = controlItemsBySourceVertex->cbegin(); it != controlItemsBySourceVertex->cend(); ++it) {
                        if (it.value() == nullptr) {
                            continue;
                        }
                        currentControlPoints.insert(it.key(), previewToSource(it.value()->pos()));
                    }

                    const QVector<MapLineSecondaryMove> moves = collectLinePreviewCoupledUpdatesForVertexDrag(feature,
                                                                                                               movedSourceVertexIndex,
                                                                                                               previousSourcePoint,
                                                                                                               newSourcePoint,
                                                                                                               currentControlPoints);
                    if (moves.isEmpty()) {
                        updateInteractiveLinePreview();
                        return;
                    }

                    *couplingGuard = true;
                    for (const MapLineSecondaryMove &move : moves) {
                        if (MapEditableGeometryVertexItem *control = controlItemsBySourceVertex->value(move.sourceVertexIndex, nullptr)) {
                            if (control != movedItem) {
                                control->setPos(sourceToPreview(move.newPoint));
                            }
                        }
                    }
                    *couplingGuard = false;
                    updateInteractiveLinePreview();
                };

                for (MapEditableGeometryVertexItem *vertexItem : *anchorItemsByOrder) {
                    if (vertexItem != nullptr) {
                        vertexItem->setMovePreviewCallback(previewLineMove);
                    }
                }
                for (MapEditableGeometryVertexItem *controlItem : controlItemsBySourceVertex->values()) {
                    if (controlItem != nullptr) {
                        controlItem->setMovePreviewCallback(previewLineMove);
                    }
                }

                if (feature.stationPoint) {
                    const QPointF headPoint = mapGeometryPointToPreview(feature.lineVertices.first().anchor, sourceBounds, previewBounds);
                    QPainterPath markerPath;
                    markerPath.moveTo(headPoint + QPointF(0.0, -12.0));
                    markerPath.lineTo(headPoint + QPointF(10.0, 6.0));
                    markerPath.lineTo(headPoint + QPointF(-10.0, 6.0));
                    markerPath.closeSubpath();

                    auto *triangle = scene->addPath(markerPath, QPen(canvasTheme.stationMarker, 1.2), QBrush(canvasTheme.stationMarker));
                    triangle->setZValue(3.5);
                    markGeometryItem(triangle);
                    makeMouseTransparent(triangle);
                }

                break;
            }
            case MapGeometryFeature::Kind::Area: {
                if (feature.vertices.size() < 3) {
                    break;
                }

                const MapEditorResolvedAreaStyle areaStyle = resolveMapEditorAreaStyle(styleCatalog,
                                                                                       feature.label,
                                                                                       feature.subtype);
                const qreal areaStrokeBase = qBound(0.8, areaStyle.strokeWidth, 24.0);
                const qreal areaStrokeWidth = qBound(0.8, areaStrokeBase * mapScale, areaStrokeBase * 1.2);
                QColor areaFillColor = areaStyle.fillColor.value_or(canvasTheme.areaFill);
                areaFillColor.setAlphaF(qBound(0.0, areaStyle.fillOpacity, 1.0));
                const QColor areaStrokeColor = areaStyle.strokeColor.value_or(canvasTheme.geometryStroke);
                QPainterPath path;
                if (feature.verticesEditable || feature.lineVertices.size() < 2) {
                    const QPointF firstPoint = mapGeometryPointToPreview(feature.vertices.first(), sourceBounds, previewBounds);
                    path.moveTo(firstPoint);
                    for (int vertexIndex = 1; vertexIndex < feature.vertices.size(); ++vertexIndex) {
                        path.lineTo(mapGeometryPointToPreview(feature.vertices.at(vertexIndex), sourceBounds, previewBounds));
                    }
                } else {
                    path = linePathForFeature(feature, sourceBounds, previewBounds);
                }
                path.closeSubpath();

                QBrush areaBrush(areaFillColor, Qt::SolidPattern);

                auto *fillItem = new MapZoomAwarePathItem(path,
                                                          styledGeometricPen(areaStrokeColor,
                                                                             areaStrokeWidth,
                                                                             areaStyle.penStyle,
                                                                             areaStyle.dashPattern,
                                                                             Qt::RoundCap,
                                                                             Qt::RoundJoin),
                                                          areaBrush);
                scene->addItem(fillItem);
                fillItem->setFlag(QGraphicsItem::ItemIsSelectable, true);
                fillItem->setZValue(2.0);
                markGeometryItem(fillItem);
                fillItem->setData(kMapSceneLineNumberRole, feature.lineNumber);
                fillItem->setData(kMapSceneSelectionSubtypeRole, kMapSceneSelectionSubtypeAreaFill);
                if (mapItemsByLine != nullptr && feature.lineNumber > 0) {
                    mapItemsByLine->insert(feature.lineNumber, fillItem);
                }

                if (areaStyle.fillPattern.has_value()) {
                    QColor fallbackPatternColor = areaStrokeColor;
                    fallbackPatternColor.setAlphaF(qBound(0.0, areaFillColor.alphaF() + 0.08, 1.0));
                    auto *patternItem = new MapZoomAwareAreaPatternItem(path,
                                                                        areaStyle.fillPattern.value(),
                                                                        fallbackPatternColor,
                                                                        feature.lineNumber);
                    scene->addItem(patternItem);
                    patternItem->setZValue(2.05);
                    markGeometryItem(patternItem);
                    makeMouseTransparent(patternItem);
                    patternItem->setData(kMapSceneLineNumberRole, feature.lineNumber);
                }
                if (feature.verticesEditable) {
                    auto areaVertexItemsByOrder = std::make_shared<QVector<MapEditableGeometryVertexItem *>>(feature.vertices.size(), nullptr);

                    for (int vertexIndex = 0; vertexIndex < feature.vertices.size(); ++vertexIndex) {
                        const QPointF vertex = feature.vertices.at(vertexIndex);
                        auto *vertexItem = new MapEditableGeometryVertexItem(feature.lineNumber,
                                                                             QStringLiteral("area"),
                                                                             vertexIndex,
                                                                             vertex,
                                                                             sourceBounds,
                                                                             previewBounds);
                        vertexItem->setRect(QRectF(-vertexRadius, -vertexRadius, vertexRadius * 2.0, vertexRadius * 2.0));
                        QColor vertexFill = feature.accent;
                        vertexFill.setAlpha(185);
                        QColor vertexOutline = feature.accent.darker(220);
                        vertexOutline.setAlpha(220);
                        vertexItem->setPen(cosmeticPen(vertexOutline, 1.0));
                        vertexItem->setBrush(QBrush(vertexFill));
                        vertexItem->setMoveCommittedCallback(recordLineAreaVertexMove);
                        scene->addItem(vertexItem);
                        vertexItem->setZValue(4.0);
                        markGeometryItem(vertexItem);
                        vertexItem->setData(kMapSceneLineNumberRole, feature.lineNumber);
                        vertexItem->setData(kMapSceneSelectionGatedRole, true);
                        vertexItem->setData(kMapSceneSelectionSubtypeRole, kMapSceneSelectionSubtypeAreaVertex);
                        vertexItem->setVisible(false);
                        areaVertexItemsByOrder->operator[](vertexIndex) = vertexItem;
                    }

                    const auto updateInteractiveAreaPreview = [fillItem,
                                                               areaVertexItemsByOrder,
                                                               feature,
                                                               sourceBounds,
                                                               previewBounds]() {
                        if (fillItem == nullptr || areaVertexItemsByOrder == nullptr || areaVertexItemsByOrder->size() < 3) {
                            return;
                        }

                        auto previewPointAt = [&](int index) -> QPointF {
                            if (index >= 0 && index < areaVertexItemsByOrder->size()) {
                                if (MapEditableGeometryVertexItem *item = areaVertexItemsByOrder->at(index)) {
                                    return item->pos();
                                }
                            }
                            if (index >= 0 && index < feature.vertices.size()) {
                                return mapGeometryPointToPreview(feature.vertices.at(index), sourceBounds, previewBounds);
                            }
                            return QPointF();
                        };

                        QPainterPath interactivePath;
                        interactivePath.moveTo(previewPointAt(0));
                        for (int index = 1; index < areaVertexItemsByOrder->size(); ++index) {
                            interactivePath.lineTo(previewPointAt(index));
                        }
                        interactivePath.closeSubpath();
                        fillItem->setPath(interactivePath);
                    };

                    for (MapEditableGeometryVertexItem *vertexItem : *areaVertexItemsByOrder) {
                        if (vertexItem != nullptr) {
                            vertexItem->setMovePreviewCallback(
                                [updateInteractiveAreaPreview](MapEditableGeometryVertexItem *,
                                                               const QPointF &,
                                                               const QPointF &,
                                                               bool) {
                                    updateInteractiveAreaPreview();
                                });
                        }
                    }
                }

                break;
            }
            }
        }
    }

    if (entries.isEmpty() && showEmptyDocumentGuides) {
        auto *emptyItem = makeMouseTransparent(scene->addText(QObject::tr("No Therion map objects were detected in this document."), QFont(QStringLiteral("Menlo"), 12)));
        emptyItem->setDefaultTextColor(canvasTheme.mutedText);
        emptyItem->setPos(previewBounds.left() + 16.0, previewBounds.top() + 44.0);
    }
}

QVector<MapGeometryFeature> collectGeometryFeatures(const QVector<TherionParsedLine> &parsedLines)
{
    QVector<MapGeometryFeature> features;
    MapGeometryFeature currentFeature;
    bool inLineBlock = false;
    bool inAreaBlock = false;
    int lineSourceVertexIndex = 0;
    QString currentLineIdentifier;
    QStringList currentAreaBorderIdentifiers;
    QHash<QString, MapGeometryFeature> lineFeaturesByIdentifier;

    auto flushCurrentFeature = [&]() {
        if (currentFeature.kind == MapGeometryFeature::Kind::Point) {
            if (currentFeature.hasAnchor) {
                features.append(currentFeature);
            }
        } else if (currentFeature.kind == MapGeometryFeature::Kind::Line) {
            if (currentFeature.vertices.size() >= 2) {
                features.append(currentFeature);
                if (!currentLineIdentifier.trimmed().isEmpty()) {
                    lineFeaturesByIdentifier.insert(currentLineIdentifier.trimmed().toLower(), currentFeature);
                }
            }
        } else if (currentFeature.kind == MapGeometryFeature::Kind::Area) {
            if (currentFeature.vertices.size() >= 3) {
                features.append(currentFeature);
            } else if (!currentAreaBorderIdentifiers.isEmpty()) {
                QVector<MapGeometryFeature::TH2LineVertex> resolvedLineVertices;
                const QVector<QPointF> resolvedVertices = areaVerticesFromReferencedLines(currentAreaBorderIdentifiers,
                                                                                          lineFeaturesByIdentifier,
                                                                                          &resolvedLineVertices);
                if (resolvedVertices.size() >= 3) {
                    currentFeature.vertices = resolvedVertices;
                    currentFeature.lineVertices = resolvedLineVertices;
                    currentFeature.closed = true;
                    currentFeature.verticesEditable = false;
                    features.append(currentFeature);
                }
            }
        }

        currentFeature = MapGeometryFeature{};
        inLineBlock = false;
        inAreaBlock = false;
        lineSourceVertexIndex = 0;
        currentLineIdentifier.clear();
        currentAreaBorderIdentifiers.clear();
    };

    for (const TherionParsedLine &parsedLine : parsedLines) {
        const QString directive = parsedLine.directive;

        if (directive == QStringLiteral("endline")) {
            if (inLineBlock) {
                flushCurrentFeature();
            }
            continue;
        }

        if (directive == QStringLiteral("endarea")) {
            if (inAreaBlock) {
                flushCurrentFeature();
            }
            continue;
        }

        if (!inLineBlock && !inAreaBlock && directive == QStringLiteral("point")) {
            const QVector<QPointF> pointTokens = pointsFromTokens(parsedLine.tokens.mid(1));
            if (pointTokens.isEmpty()) {
                continue;
            }

            MapGeometryFeature feature;
            feature.kind = MapGeometryFeature::Kind::Point;
            feature.lineNumber = parsedLine.lineNumber;
            feature.category = mapEntryCategoryForLine(parsedLine);
            feature.label = mapEntryTitleForLine(parsedLine);
            feature.subtype = optionValue(parsedLine.tokens, QStringLiteral("-subtype"));
            feature.subtitle = mapEntrySubtitleForLine(parsedLine);
            feature.accent = mapEntryAccentForCategory(feature.category);
            feature.sourceAnchor = pointTokens.first();
            feature.anchor = feature.sourceAnchor;
            feature.hasAnchor = true;
            feature.hasSourceAnchor = true;
            if (const std::optional<qreal> orientation =
                    linePointNumericOptionValue(parsedLine,
                                                QStringList{QStringLiteral("-orientation"),
                                                            QStringLiteral("orientation"),
                                                            QStringLiteral("-orient"),
                                                            QStringLiteral("orient")})) {
                feature.orientationDegrees = normalizedSceneOrientationDegrees(orientation.value());
            }
            feature.stationPoint = false;
            features.append(feature);
            continue;
        }

        if (!inLineBlock && !inAreaBlock && directive == QStringLiteral("station")) {
            const QVector<QPointF> pointTokens = pointsFromTokens(parsedLine.tokens.mid(1));
            if (pointTokens.isEmpty()) {
                continue;
            }

            MapGeometryFeature feature;
            feature.kind = MapGeometryFeature::Kind::Point;
            feature.lineNumber = parsedLine.lineNumber;
            feature.category = mapEntryCategoryForLine(parsedLine);
            feature.label = mapEntryTitleForLine(parsedLine);
            feature.subtype = optionValue(parsedLine.tokens, QStringLiteral("-subtype"));
            feature.subtitle = mapEntrySubtitleForLine(parsedLine);
            feature.accent = mapEntryAccentForCategory(feature.category);
            feature.sourceAnchor = pointTokens.first();
            feature.anchor = feature.sourceAnchor;
            feature.hasAnchor = true;
            feature.hasSourceAnchor = true;
            feature.stationPoint = true;
            features.append(feature);
            continue;
        }

        if (!inLineBlock && !inAreaBlock && directive == QStringLiteral("line")) {
            flushCurrentFeature();
            currentFeature.kind = MapGeometryFeature::Kind::Line;
            currentFeature.lineNumber = parsedLine.lineNumber;
            currentFeature.category = mapEntryCategoryForLine(parsedLine);
            currentFeature.label = mapEntryTitleForLine(parsedLine);
            currentFeature.subtype = optionValue(parsedLine.tokens, QStringLiteral("-subtype"));
            currentFeature.subtitle = mapEntrySubtitleForLine(parsedLine);
            currentFeature.accent = mapEntryAccentForCategory(currentFeature.category);
            currentFeature.closed = lineOptionToggleValue(parsedLine, QStringLiteral("-close")).value_or(false);
            currentFeature.reversed = lineOptionToggleValue(parsedLine, QStringLiteral("-reverse")).value_or(false);
            currentLineIdentifier = optionValue(parsedLine.tokens, QStringLiteral("-id"));
            appendLineDataPoints(&currentFeature, sourceCoordinatePointsFromLine(parsedLine, 1, &lineSourceVertexIndex));
            applyLinePointOptionsFromLine(parsedLine, &currentFeature);
            inLineBlock = true;
            continue;
        }

        if (!inLineBlock && !inAreaBlock && directive == QStringLiteral("area")) {
            flushCurrentFeature();
            currentFeature.kind = MapGeometryFeature::Kind::Area;
            currentFeature.lineNumber = parsedLine.lineNumber;
            currentFeature.category = mapEntryCategoryForLine(parsedLine);
            currentFeature.label = mapEntryTitleForLine(parsedLine);
            currentFeature.subtype = optionValue(parsedLine.tokens, QStringLiteral("-subtype"));
            currentFeature.subtitle = mapEntrySubtitleForLine(parsedLine);
            currentFeature.accent = mapEntryAccentForCategory(currentFeature.category);
            currentFeature.vertices.append(pointsFromTokens(parsedLine.tokens.mid(1)));
            appendAreaReferenceIdentifiers(parsedLine, 1, &currentAreaBorderIdentifiers);
            inAreaBlock = true;
            continue;
        }

        if (inLineBlock) {
            if (directive == QStringLiteral("smooth")
                && parsedLine.tokens.size() >= 2
                && parsedLine.tokens.at(1).compare(QStringLiteral("off"), Qt::CaseInsensitive) == 0) {
                if (!currentFeature.lineVertices.isEmpty()) {
                    currentFeature.lineVertices.last().isSmooth = false;
                }
                continue;
            }

            appendLineDataPoints(&currentFeature, sourceCoordinatePointsFromLine(parsedLine, 0, &lineSourceVertexIndex));
            applyLinePointOptionsFromLine(parsedLine, &currentFeature);
            continue;
        }

        if (inAreaBlock) {
            currentFeature.vertices.append(pointsFromTokens(parsedLine.tokens));
            appendAreaReferenceIdentifiers(parsedLine, 0, &currentAreaBorderIdentifiers);
            continue;
        }
    }

    if (inLineBlock || inAreaBlock) {
        flushCurrentFeature();
    }

    return features;
}

} // namespace TherionStudio
