#include "MapEditorSceneSupport.h"
#include "MapEditorSceneInternals.h"
#include "MapEditorSceneThemePolicy.h"
#include "MapEditorLineDecorationItem.h"
#include "MapEditorObjectStyleCatalog.h"
#include "MapEditorPointSymbolGeometry.h"
#include "MapEditorReferencedAreaResolver.h"

#include <QFont>
#include <QFontMetricsF>
#include <QGraphicsEllipseItem>
#include <QGraphicsLineItem>
#include <QGraphicsPathItem>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QPainter>
#include <QPainterPath>
#include <QRegularExpression>
#include <QSet>
#include <QStaticText>
#include <QTransform>

#include "../../../core/TherionDocumentParser.h"
#include "../../../core/TherionCommandLineModel.h"
#include "../../../core/Th2GeometryProjection.h"
#include "../../../core/TherionSourceLogicalDocument.h"
#include "../../../core/TherionTokenRules.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <optional>
#include <utility>

namespace TherionStudio {
namespace {
constexpr qreal kMinimumMapGeometryStrokeLogicalPx = 1.15;

struct LineControlConnectorBinding
{
    int anchorVertexOrder = -1;
    int controlSourceVertexIndex = -1;
    QGraphicsLineItem *lineItem = nullptr;
};

struct StyledPathItemBinding
{
    int styledPathIndex = -1;
    QGraphicsPathItem *pathItem = nullptr;
};

struct StyledLineDecorationBinding
{
    int styledPathIndex = -1;
    MapEditorLineDecorationItem *decorationItem = nullptr;
};

struct MapCanvasTheme
{
    bool lightMode = false;
    QColor canvasBorder;
    QColor canvasFill;
    QColor geometryStroke;
    QColor areaFill;
    QColor labelText;
    QColor mutedText;
    QColor pointHandleStroke;
    QColor pointHandleFill;
    QColor controlConnector;
    QColor controlHandleStroke;
    QColor controlHandleFill;
};

struct AreaRenderZValues
{
    qreal fill = 2.0;
    qreal pattern = 2.05;
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

qreal paintDevicePixelRatio(QPainter *painter)
{
    return painter != nullptr && painter->device() != nullptr
        ? qMax<qreal>(1.0, painter->device()->devicePixelRatioF())
        : 1.0;
}

qreal cosmeticStrokeWidthForPaintDevice(QPainter *painter, qreal requestedLogicalWidth)
{
    return qMax<qreal>(kMinimumMapGeometryStrokeLogicalPx, requestedLogicalWidth)
        * paintDevicePixelRatio(painter);
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
        setAcceptHoverEvents(true);
    }

    void setHoverInteractionOverlayEnabled(bool enabled)
    {
        hoverInteractionOverlayEnabled_ = enabled;
        update();
    }

    void setHoverInteractionOverlayStroke(qreal minimumWidth, qreal extraWidth)
    {
        hoverOverlayMinimumWidth_ = qMax<qreal>(0.1, minimumWidth);
        hoverOverlayExtraWidth_ = qMax<qreal>(0.0, extraWidth);
        update();
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
        drawPen.setWidthF(cosmeticStrokeWidthForPaintDevice(painter, basePen_.widthF() * widthScale));

        painter->save();
        painter->setPen(drawPen);
        painter->setBrush(brush());
        painter->drawPath(path());
        drawInteractionOverlay(painter, drawPen, option);
        painter->restore();
    }

    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override
    {
        hoverActive_ = true;
        QGraphicsPathItem::hoverEnterEvent(event);
        update();
    }

    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override
    {
        hoverActive_ = false;
        QGraphicsPathItem::hoverLeaveEvent(event);
        update();
    }

private:
    void drawInteractionOverlay(QPainter *painter,
                                const QPen &drawPen,
                                const QStyleOptionGraphicsItem *option) const
    {
        if (painter == nullptr) {
            return;
        }

        const bool selected = (option != nullptr && (option->state & QStyle::State_Selected))
            || data(kMapSceneInteractionSelectionRole).toBool();
        const bool hovered = data(kMapSceneInteractionHoverRole).toBool();
        if (!selected && (!hovered || !hoverInteractionOverlayEnabled_)) {
            return;
        }

        const bool baseStrokeVisible = basePen_.style() != Qt::NoPen
            && basePen_.color().alpha() > 0
            && basePen_.widthF() > 0.0;
        if (hovered && !selected && !baseStrokeVisible) {
            return;
        }

        QColor overlayColor = selected ? mapEditorInteractionSelectionColor() : mapEditorInteractionHoverColor();
        overlayColor.setAlpha(selected ? 235 : 220);
        const qreal devicePixelRatio = paintDevicePixelRatio(painter);
        const qreal overlayMinimumWidth = selected ? selectionOverlayMinimumWidth_ : hoverOverlayMinimumWidth_;
        const qreal overlayExtraWidth = selected ? selectionOverlayExtraWidth_ : hoverOverlayExtraWidth_;
        const qreal overlayWidth = qMax<qreal>(overlayMinimumWidth * devicePixelRatio,
                                               drawPen.widthF() + (overlayExtraWidth * devicePixelRatio));
        QPen overlayPen(overlayColor,
                        overlayWidth,
                        Qt::SolidLine,
                        Qt::RoundCap,
                        Qt::RoundJoin);
        overlayPen.setCosmetic(true);
        painter->setPen(overlayPen);
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(path());
    }

    QPen basePen_;
    bool hoverActive_ = false;
    bool hoverInteractionOverlayEnabled_ = true;
    qreal hoverOverlayMinimumWidth_ = 2.6;
    qreal hoverOverlayExtraWidth_ = 1.2;
    qreal selectionOverlayMinimumWidth_ = 2.6;
    qreal selectionOverlayExtraWidth_ = 1.8;
};

class MapPathLabelItem final : public QGraphicsItem
{
public:
    MapPathLabelItem(QString text, QPainterPath path, const QFont &font, const QColor &color)
        : text_(std::move(text))
        , path_(std::move(path))
        , font_(font)
        , color_(color)
    {
        setAcceptedMouseButtons(Qt::NoButton);
    }

    QRectF boundingRect() const override
    {
        const qreal margin = QFontMetricsF(font_).height() * 2.0;
        return path_.boundingRect().adjusted(-margin, -margin, margin, margin);
    }

    void setPath(const QPainterPath &path)
    {
        if (path_ == path) {
            return;
        }
        prepareGeometryChange();
        path_ = path;
        update();
    }

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override
    {
        Q_UNUSED(option);
        Q_UNUSED(widget);
        if (painter == nullptr || text_.isEmpty()) {
            return;
        }

        const qreal pathLength = path_.length();
        if (pathLength <= 1e-6) {
            return;
        }

        const QFontMetricsF metrics(font_);
        QVector<qreal> advances;
        advances.reserve(text_.size());
        qreal naturalTextWidth = 0.0;
        for (const QChar character : text_) {
            const qreal advance = qMax<qreal>(metrics.horizontalAdvance(QString(character)), 0.1);
            advances.append(advance);
            naturalTextWidth += advance;
        }
        if (naturalTextWidth <= 1e-6) {
            return;
        }

        // Therion's MetaPost l_label lays glyphs along the whole path and scales
        // text down only when it would not fit.
        const qreal fontScale = pathLength < naturalTextWidth ? pathLength / naturalTextWidth : 1.0;
        const qreal effectiveTextWidth = naturalTextWidth * fontScale;
        const qreal pathSpacingScale = effectiveTextWidth > 1e-6 ? pathLength / effectiveTextWidth : 1.0;

        painter->save();
        painter->setFont(font_);
        painter->setPen(color_);

        qreal textCursor = 0.0;
        for (int index = 0; index < text_.size(); ++index) {
            const qreal effectiveAdvance = advances.at(index) * fontScale;
            const qreal distance = qBound<qreal>(0.0,
                                                 (textCursor + effectiveAdvance / 2.0) * pathSpacingScale,
                                                 pathLength);
            const qreal percent = path_.percentAtLength(distance);
            const QPointF position = path_.pointAtPercent(percent);

            const qreal beforeDistance = qMax<qreal>(0.0, distance - 0.5);
            const qreal afterDistance = qMin<qreal>(pathLength, distance + 0.5);
            const QPointF before = path_.pointAtPercent(path_.percentAtLength(beforeDistance));
            const QPointF after = path_.pointAtPercent(path_.percentAtLength(afterDistance));
            const QPointF tangent = after - before;
            constexpr qreal kPi = 3.14159265358979323846;
            const qreal angleDegrees = std::atan2(tangent.y(), tangent.x()) * 180.0 / kPi;

            painter->save();
            painter->translate(position);
            painter->rotate(angleDegrees);
            painter->scale(fontScale, fontScale);
            painter->drawText(QPointF(-advances.at(index) / 2.0,
                                      metrics.ascent() - metrics.height() / 2.0),
                              QString(text_.at(index)));
            painter->restore();
            textCursor += effectiveAdvance;
        }

        painter->restore();
    }

private:
    QString text_;
    QPainterPath path_;
    QFont font_;
    QColor color_;
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

AreaRenderZValues areaRenderZValues(MapGeometryAreaPlace place)
{
    switch (place) {
    case MapGeometryAreaPlace::Bottom:
        return {1.15, 1.20};
    case MapGeometryAreaPlace::Default:
        return {1.35, 1.40};
    case MapGeometryAreaPlace::Top:
        return {2.85, 2.90};
    }

    return {};
}

std::optional<QColor> closedLineFillColor(const MapEditorResolvedLineStyle &style,
                                          const MapCanvasTheme &canvasTheme)
{
    switch (style.closedFill.mode) {
    case MapEditorLineClosedFillMode::None:
        return std::nullopt;
    case MapEditorLineClosedFillMode::Background: {
        QColor color = canvasTheme.canvasFill;
        color.setAlpha(255);
        return color;
    }
    case MapEditorLineClosedFillMode::Color:
        return style.closedFill.color;
    }

    return std::nullopt;
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
        const qreal sceneSymbolSize = qBound<qreal>(0.6, pattern_.size / safeLod, 64.0);
        const qreal sceneSymbolSizeJitter = qBound<qreal>(0.0, pattern_.sizeJitter / safeLod, 64.0);
        const qreal configuredSceneOffsetJitter = qBound<qreal>(0.0, pattern_.offsetJitter / safeLod, 64.0);
        const qreal sceneOffsetJitter =
            pattern_.kind == MapEditorFillPatternKind::Dots
                    && pattern_.dotPlacement == MapEditorAreaDotPlacement::Scatter
                    && pattern_.offsetJitter <= 0.0
                ? sceneSpacing * 0.48
                : configuredSceneOffsetJitter;
        const int seedValue = pattern_.seed.value_or(fallbackSeed_);
        const quint32 seed = static_cast<quint32>(seedValue == 0 ? 1 : seedValue);
        const QRectF bounds = path().boundingRect();
        QRectF paintBounds = bounds;
        if (option != nullptr && option->exposedRect.isValid()) {
            const qreal margin = sceneSpacing + sceneOffsetJitter + sceneSymbolSize + sceneSymbolSizeJitter + sceneStrokeWidth + 2.0;
            paintBounds = bounds.intersected(option->exposedRect.adjusted(-margin, -margin, margin, margin));
            if (paintBounds.isEmpty()) {
                painter->restore();
                return;
            }
        }

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
            const QPointF anchorCenter = bounds.center();
            const qreal paintExtent = std::hypot(paintBounds.width(), paintBounds.height()) * 1.8 + sceneSpacing + sceneOffsetJitter;
            const qreal baseRadians = baseAngle * (kPi / 180.0);
            const QPointF baseDirection(std::cos(baseRadians), std::sin(baseRadians));
            const QPointF baseNormal(-baseDirection.y(), baseDirection.x());
            const QPointF corners[] = {
                paintBounds.topLeft(),
                paintBounds.topRight(),
                paintBounds.bottomLeft(),
                paintBounds.bottomRight()
            };
            qreal minProjection = std::numeric_limits<qreal>::max();
            qreal maxProjection = std::numeric_limits<qreal>::lowest();
            for (const QPointF &corner : corners) {
                const QPointF delta = corner - anchorCenter;
                const qreal projection = QPointF::dotProduct(delta, baseNormal);
                minProjection = qMin(minProjection, projection);
                maxProjection = qMax(maxProjection, projection);
            }
            const qreal jitterBandMargin = qMax<qreal>(2.0, std::ceil((sceneOffsetJitter + sceneSpacing) / sceneSpacing));
            const int firstBand = static_cast<int>(std::floor(minProjection / sceneSpacing - jitterBandMargin));
            const int lastBand = static_cast<int>(std::ceil(maxProjection / sceneSpacing + jitterBandMargin));
            for (int band = firstBand; band <= lastBand; ++band) {
                const qreal jitterOffset = sceneOffsetJitter <= 0.0
                    ? 0.0
                    : stableRandomSigned(seed, orientationIndex, band, 0x20411U) * sceneOffsetJitter;
                const qreal jitterAngle = pattern_.angleJitter <= 0.0
                    ? 0.0
                    : stableRandomSigned(seed, orientationIndex, band, 0x8b2f1U) * pattern_.angleJitter;
                const qreal radians = (baseAngle + jitterAngle) * (kPi / 180.0);
                const QPointF direction(std::cos(radians), std::sin(radians));
                const QPointF normal(-direction.y(), direction.x());
                const QPointF shiftedCenter = anchorCenter + normal * ((static_cast<qreal>(band) * sceneSpacing) + jitterOffset);
                const QPointF paintCenter = shiftedCenter
                    + direction * QPointF::dotProduct(paintBounds.center() - shiftedCenter, direction);
                painter->drawLine(paintCenter - (direction * paintExtent), paintCenter + (direction * paintExtent));
            }
        };

        if (pattern_.kind == MapEditorFillPatternKind::Hatch || pattern_.kind == MapEditorFillPatternKind::CrossHatch) {
            drawHatch(pattern_.angle, 0);
            if (pattern_.kind == MapEditorFillPatternKind::CrossHatch) {
                drawHatch(pattern_.angle + 90.0, 1);
            }
        } else if (pattern_.kind == MapEditorFillPatternKind::Dots) {
            const QColor baseSymbolColor = pattern_.strokeColor.value_or(fallbackColor_);
            const qreal alphaByZoom = 1.0 / std::pow(lodZoomIn, 0.20);
            const auto scaledSymbolColor = [&](QColor color) {
                const qreal baseAlpha = color.alphaF();
                if (baseAlpha <= 0.0) {
                    color.setAlphaF(0.0);
                    return color;
                }
                color.setAlphaF(qBound(0.08, baseAlpha * alphaByZoom * 0.88, 1.0));
                return color;
            };

            const bool scatterPlacement = pattern_.dotPlacement == MapEditorAreaDotPlacement::Scatter;
            const qreal startPadding = scatterPlacement ? sceneSpacing : 0.0;
            const qreal centerBias = scatterPlacement ? 0.5 : 0.0;
            const qreal startX = (std::floor(paintBounds.left() / sceneSpacing) * sceneSpacing) - startPadding;
            const qreal startY = (std::floor(paintBounds.top() / sceneSpacing) * sceneSpacing) - startPadding;
            const int paddingCells = scatterPlacement ? 5 : 3;
            const int cols = qBound(1, static_cast<int>(std::ceil(paintBounds.width() / sceneSpacing)) + paddingCells, 1200);
            const int rows = qBound(1, static_cast<int>(std::ceil(paintBounds.height() / sceneSpacing)) + paddingCells, 1200);

            for (int row = 0; row < rows; ++row) {
                for (int col = 0; col < cols; ++col) {
                    const int index = row * 4096 + col;
                    const qreal jitterX = sceneOffsetJitter <= 0.0
                        ? 0.0
                        : stableRandomSigned(seed, 2, index, 0x14f21U) * sceneOffsetJitter;
                    const qreal jitterY = sceneOffsetJitter <= 0.0
                        ? 0.0
                        : stableRandomSigned(seed, 3, index, 0x6a223U) * sceneOffsetJitter;
                    const QPointF dotCenter(startX + ((static_cast<qreal>(col) + centerBias) * sceneSpacing) + jitterX,
                                            startY + ((static_cast<qreal>(row) + centerBias) * sceneSpacing) + jitterY);
                    const qreal symbolSizeJitter = sceneSymbolSizeJitter <= 0.0
                        ? 0.0
                        : stableRandomSigned(seed, 5, index, 0x3c6efU) * sceneSymbolSizeJitter;
                    const qreal currentSceneSymbolSize =
                        qBound<qreal>(0.6, sceneSymbolSize + symbolSizeJitter, 64.0);
                    const auto applySymbolPaint = [&](bool usesFill,
                                                      const std::optional<QColor> &partStrokeColor,
                                                      const std::optional<QColor> &partFillColor,
                                                      const std::optional<qreal> &partStrokeWidth) {
                        const QColor strokeColor =
                            scaledSymbolColor(partStrokeColor.value_or(partFillColor.value_or(baseSymbolColor)));
                        const QColor fillColor =
                            scaledSymbolColor(partFillColor.value_or(partStrokeColor.value_or(baseSymbolColor)));
                        const bool drawFill = usesFill && fillColor.alpha() > 0;
                        const bool drawStroke = !usesFill
                            || partStrokeColor.has_value()
                            || partStrokeWidth.has_value();
                        if (usesFill) {
                            painter->setBrush(drawFill ? QBrush(fillColor) : Qt::NoBrush);
                            if (!drawStroke) {
                                painter->setPen(Qt::NoPen);
                                return;
                            }
                        } else {
                            painter->setBrush(Qt::NoBrush);
                        }
                        const qreal effectiveStrokeWidth = partStrokeWidth.has_value()
                            ? partStrokeWidth.value() / safeLod
                            : currentSceneSymbolSize * 0.18;
                        QPen symbolPen(strokeColor,
                                       qBound<qreal>(0.16, effectiveStrokeWidth, 4.0),
                                       Qt::SolidLine,
                                       Qt::RoundCap,
                                       Qt::RoundJoin);
                        painter->setPen(symbolPen);
                    };
                    const QRectF symbolRect(dotCenter.x() - (currentSceneSymbolSize / 2.0),
                                            dotCenter.y() - (currentSceneSymbolSize / 2.0),
                                            currentSceneSymbolSize,
                                            currentSceneSymbolSize);
                    QTransform symbolTransform;
                    if (pattern_.angleJitter > 0.0) {
                        const qreal jitterAngle =
                            stableRandomSigned(seed, 4, index, 0x92d41U) * pattern_.angleJitter;
                        symbolTransform.translate(dotCenter.x(), dotCenter.y());
                        symbolTransform.rotate(jitterAngle);
                        symbolTransform.translate(-dotCenter.x(), -dotCenter.y());
                    }
                    const auto drawPartPath = [&](const QPainterPath &path,
                                                  bool usesFill,
                                                  const std::optional<QColor> &partStrokeColor,
                                                  const std::optional<QColor> &partFillColor,
                                                  const std::optional<qreal> &partStrokeWidth) {
                        applySymbolPaint(usesFill, partStrokeColor, partFillColor, partStrokeWidth);
                        painter->drawPath(symbolTransform.map(path));
                    };
                    const auto drawFallbackPath = [&](const QPainterPath &path, bool usesFill) {
                        if (usesFill) {
                            painter->setPen(Qt::NoPen);
                            painter->setBrush(scaledSymbolColor(baseSymbolColor));
                            return;
                        }
                        QPen symbolPen(scaledSymbolColor(baseSymbolColor),
                                       qBound<qreal>(0.16, currentSceneSymbolSize * 0.18, 4.0),
                                       Qt::SolidLine,
                                       Qt::RoundCap,
                                       Qt::RoundJoin);
                        painter->setPen(symbolPen);
                        painter->setBrush(Qt::NoBrush);
                    };
                    if (pattern_.symbolParts.isEmpty()) {
                        const QPainterPath path = mapEditorPointSymbolPath(pattern_.symbol, symbolRect);
                        drawFallbackPath(path, mapEditorPointSymbolUsesFill(pattern_.symbol));
                        painter->drawPath(symbolTransform.map(path));
                    } else {
                        for (const MapEditorPointSymbolPart &part : pattern_.symbolParts) {
                            drawPartPath(mapEditorPointSymbolPartPath(part,
                                                                      symbolRect,
                                                                      qMax<qreal>(0.001, pattern_.size)),
                                         mapEditorSymbolPartUsesFill(part),
                                         part.strokeColor,
                                         part.fillColor,
                                         part.strokeWidth);
                        }
                    }
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

    const MapEditorSceneThemeColors colors = mapEditorSceneThemeColors();

    MapCanvasTheme theme;
    theme.lightMode = true;
    theme.canvasBorder = colors.canvasBorder;
    theme.canvasFill = colors.canvasFill;
    theme.geometryStroke = colors.geometryStroke;
    theme.areaFill = colors.areaFill;
    theme.labelText = colors.labelText;
    theme.mutedText = colors.mutedText;
    theme.pointHandleStroke = colors.pointHandleStroke;
    theme.pointHandleFill = colors.pointHandleFill;
    theme.controlConnector = colors.controlConnector;
    theme.controlHandleStroke = colors.controlHandleStroke;
    theme.controlHandleFill = colors.controlHandleFill;
    return theme;
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

        if (firstNonQuotedIndex >= 0 && !TherionTokenRules::isNumericToken(parsedLine.tokens.at(firstNonQuotedIndex))) {
            return points;
        }
    }

    if (firstTokenIndex < parsedLine.tokens.size()) {
        const QString firstToken = parsedLine.tokens.at(firstTokenIndex);
        if (firstTokenIndex == 0
            && firstToken.startsWith(QLatin1Char('-'))
            && !TherionTokenRules::isNumericToken(firstToken)) {
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
            && !TherionTokenRules::isNumericToken(token)) {
            if (index + 1 < parsedLine.tokens.size()) {
                const QString nextToken = parsedLine.tokens.at(index + 1);
                if (!TherionTokenRules::tokenStartsOption(nextToken)) {
                    skipOptionValueToken = true;
                }
            }
            continue;
        }

        const bool numeric = TherionTokenRules::isNumericToken(token);
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

void appendLineDataPoints(MapGeometryFeature *feature,
                          const QVector<SourceCoordinatePoint> &linePoints,
                          const QString &activeSegmentSubtype)
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
            segment.subtype = activeSegmentSubtype;
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
            segment.subtype = activeSegmentSubtype;
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
            segment.subtype = activeSegmentSubtype;
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
        segment.subtype = activeSegmentSubtype;
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

    if (feature.closed && feature.lineVertices.size() >= 2) {
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
        path.closeSubpath();
    }

    return path;
}

struct StyledLinePath
{
    QString subtype;
    QPainterPath path;
};

int lineSegmentStartVertexIndex(const MapGeometryFeature &feature, int segmentIndex)
{
    if (segmentIndex < 0 || segmentIndex >= feature.lineSegments.size()) {
        return -1;
    }
    if (segmentIndex == 0) {
        return feature.lineVertices.isEmpty() ? -1 : 0;
    }

    return feature.lineSegments.at(segmentIndex - 1).endVertexIndex;
}

bool appendSegmentToPath(QPainterPath *path,
                         const MapGeometryFeature &feature,
                         const MapGeometryFeature::LineSegment &segment,
                         int startVertexIndex,
                         const std::function<QPointF(const QPointF &)> &toPreview)
{
    if (path == nullptr
        || startVertexIndex < 0
        || startVertexIndex >= feature.lineVertices.size()
        || segment.endVertexIndex < 0
        || segment.endVertexIndex >= feature.lineVertices.size()) {
        return false;
    }

    const MapGeometryFeature::TH2LineVertex &startVertex = feature.lineVertices.at(startVertexIndex);
    const MapGeometryFeature::TH2LineVertex &endVertex = feature.lineVertices.at(segment.endVertexIndex);
    if (path->elementCount() == 0) {
        path->moveTo(toPreview(startVertex.anchor));
    }

    const QPointF cp1 = startVertex.outgoingControl.value_or(startVertex.anchor);
    const QPointF cp2 = endVertex.incomingControl.value_or(endVertex.anchor);
    const bool hasCurveHandle =
        segment.type == MapGeometryFeature::LineSegment::Type::Cubic
        || startVertex.outgoingControl.has_value()
        || endVertex.incomingControl.has_value();
    if (hasCurveHandle) {
        path->cubicTo(toPreview(cp1), toPreview(cp2), toPreview(endVertex.anchor));
    } else {
        path->lineTo(toPreview(endVertex.anchor));
    }

    return true;
}

QVector<StyledLinePath> styledLinePathsForFeature(const MapGeometryFeature &feature,
                                                  const QRectF &sourceBounds,
                                                  const QRectF &previewBounds)
{
    QVector<StyledLinePath> paths;
    if (feature.lineVertices.size() < 2 || feature.lineSegments.isEmpty()) {
        return paths;
    }

    auto toPreview = [&](const QPointF &point) {
        return mapGeometryPointToPreview(point, sourceBounds, previewBounds);
    };
    StyledLinePath currentPath;
    bool hasCurrentPath = false;
    for (int segmentIndex = 0; segmentIndex < feature.lineSegments.size(); ++segmentIndex) {
        const MapGeometryFeature::LineSegment &segment = feature.lineSegments.at(segmentIndex);
        const int startVertexIndex = lineSegmentStartVertexIndex(feature, segmentIndex);
        const QString segmentSubtype = segment.subtype.trimmed().isEmpty()
            ? feature.subtype.trimmed().toLower()
            : segment.subtype.trimmed().toLower();

        if (!hasCurrentPath || currentPath.subtype != segmentSubtype) {
            if (hasCurrentPath && currentPath.path.elementCount() > 1) {
                paths.append(currentPath);
            }
            currentPath = StyledLinePath{};
            currentPath.subtype = segmentSubtype;
            hasCurrentPath = true;
        }

        if (!appendSegmentToPath(&currentPath.path,
                                 feature,
                                 segment,
                                 startVertexIndex,
                                 toPreview)) {
            continue;
        }
    }

    if (hasCurrentPath && currentPath.path.elementCount() > 1) {
        paths.append(currentPath);
    }

    return paths;
}

bool lineSegmentNeedsGuideSpine(const MapEditorResolvedLineStyle &style,
                                const QString &rawType,
                                const QString &subtype)
{
    if (style.guideSpineVisible || !style.decorations.isEmpty()) {
        return true;
    }

    return rawType.trimmed().compare(QStringLiteral("wall"), Qt::CaseInsensitive) == 0
        && subtype.trimmed().compare(QStringLiteral("blocks"), Qt::CaseInsensitive) == 0;
}

struct WallClipPathCandidate
{
    QPainterPath path;
    QPointF start;
    QPointF end;
    qreal length = 0.0;
};

std::optional<WallClipPathCandidate> wallClipPathCandidateForFeature(const MapGeometryFeature &feature,
                                                                     const QRectF &sourceBounds,
                                                                     const QRectF &previewBounds)
{
    QPainterPath path = linePathForFeature(feature, sourceBounds, previewBounds);
    if (path.elementCount() < 2 || path.boundingRect().isEmpty()) {
        return std::nullopt;
    }

    const QPainterPath::Element first = path.elementAt(0);
    WallClipPathCandidate candidate;
    candidate.path = path;
    candidate.start = QPointF(first.x, first.y);
    candidate.end = path.currentPosition();
    candidate.length = qMax<qreal>(0.0, path.length());
    return candidate;
}

qreal pointDistanceSquared(const QPointF &first, const QPointF &second)
{
    const QPointF delta = first - second;
    return (delta.x() * delta.x()) + (delta.y() * delta.y());
}

QPainterPath stitchedOpenWallClipPath(QVector<WallClipPathCandidate> candidates)
{
    if (candidates.size() < 2) {
        return QPainterPath();
    }

    int startIndex = 0;
    for (int index = 1; index < candidates.size(); ++index) {
        if (candidates.at(index).length > candidates.at(startIndex).length) {
            startIndex = index;
        }
    }

    WallClipPathCandidate current = candidates.takeAt(startIndex);
    QPainterPath stitched = current.path;
    const QPointF firstPoint = current.start;
    QPointF currentEnd = current.end;

    while (!candidates.isEmpty()) {
        int bestIndex = -1;
        bool reverseBest = false;
        qreal bestDistance = std::numeric_limits<qreal>::max();
        for (int index = 0; index < candidates.size(); ++index) {
            const WallClipPathCandidate &candidate = candidates.at(index);
            const qreal startDistance = pointDistanceSquared(currentEnd, candidate.start);
            if (startDistance < bestDistance) {
                bestDistance = startDistance;
                bestIndex = index;
                reverseBest = false;
            }
            const qreal endDistance = pointDistanceSquared(currentEnd, candidate.end);
            if (endDistance < bestDistance) {
                bestDistance = endDistance;
                bestIndex = index;
                reverseBest = true;
            }
        }

        if (bestIndex < 0) {
            break;
        }

        const WallClipPathCandidate next = candidates.takeAt(bestIndex);
        const QPainterPath nextPath = reverseBest ? next.path.toReversed() : next.path;
        stitched.connectPath(nextPath);
        currentEnd = reverseBest ? next.start : next.end;
    }

    if (pointDistanceSquared(firstPoint, currentEnd) > 1.0) {
        return QPainterPath();
    }
    stitched.closeSubpath();
    stitched.setFillRule(Qt::OddEvenFill);
    return stitched.boundingRect().isEmpty() ? QPainterPath() : stitched;
}

QHash<int, QPainterPath> scrapClipPathsForFeatures(const QVector<MapGeometryFeature> &features,
                                                   const QRectF &sourceBounds,
                                                   const QRectF &previewBounds)
{
    QHash<int, QVector<WallClipPathCandidate>> openWallsByScrap;
    QHash<int, QVector<QPainterPath>> closedWallsByScrap;

    for (const MapGeometryFeature &feature : features) {
        if (feature.kind != MapGeometryFeature::Kind::Line
            || feature.scrapLineNumber <= 0
            || feature.label.trimmed().toLower() != QStringLiteral("wall")
            || feature.lineVertices.size() < 2) {
            continue;
        }

        const std::optional<WallClipPathCandidate> candidate =
            wallClipPathCandidateForFeature(feature, sourceBounds, previewBounds);
        if (!candidate.has_value()) {
            continue;
        }

        if (feature.closed && feature.lineVertices.size() >= 2) {
            QPainterPath closed = candidate->path;
            closed.closeSubpath();
            closed.setFillRule(Qt::OddEvenFill);
            if (!closed.boundingRect().isEmpty()) {
                closedWallsByScrap[feature.scrapLineNumber].append(closed);
            }
        } else {
            openWallsByScrap[feature.scrapLineNumber].append(candidate.value());
        }
    }

    QHash<int, QPainterPath> clipPathsByScrap;
    const QList<int> closedScraps = closedWallsByScrap.keys();
    for (const int scrapLineNumber : closedScraps) {
        QPainterPath combined;
        combined.setFillRule(Qt::OddEvenFill);
        for (const QPainterPath &path : std::as_const(closedWallsByScrap[scrapLineNumber])) {
            combined.addPath(path);
        }
        if (!combined.boundingRect().isEmpty()) {
            clipPathsByScrap.insert(scrapLineNumber, combined);
        }
    }

    const QList<int> openScraps = openWallsByScrap.keys();
    for (const int scrapLineNumber : openScraps) {
        if (clipPathsByScrap.contains(scrapLineNumber)) {
            continue;
        }
        const QPainterPath stitched = stitchedOpenWallClipPath(openWallsByScrap.value(scrapLineNumber));
        if (!stitched.boundingRect().isEmpty()) {
            clipPathsByScrap.insert(scrapLineNumber, stitched);
        }
    }

    return clipPathsByScrap;
}

QVector<MapEditorLineDecorationVertex> lineDecorationVerticesForFeature(const MapGeometryFeature &feature,
                                                                        const QRectF &sourceBounds,
                                                                        const QRectF &previewBounds)
{
    QVector<MapEditorLineDecorationVertex> vertices;
    if (feature.lineVertices.isEmpty()) {
        return vertices;
    }

    auto toPreview = [&](const QPointF &point) {
        return mapGeometryPointToPreview(point, sourceBounds, previewBounds);
    };

    auto appendVertex = [&vertices](const MapGeometryFeature::TH2LineVertex &sourceVertex,
                                    const QPointF &previewAnchor,
                                    qreal pathDistance) {
        MapEditorLineDecorationVertex vertex;
        vertex.anchor = previewAnchor;
        vertex.pathDistance = pathDistance;
        vertex.orientationDegrees = sourceVertex.orientationDegrees;
        vertex.leftSize = sourceVertex.leftSize;
        vertices.append(vertex);
    };

    qreal pathDistance = 0.0;
    appendVertex(feature.lineVertices.first(), toPreview(feature.lineVertices.first().anchor), pathDistance);
    for (int index = 1; index < feature.lineVertices.size(); ++index) {
        const MapGeometryFeature::TH2LineVertex &previousVertex = feature.lineVertices.at(index - 1);
        const MapGeometryFeature::TH2LineVertex &currentVertex = feature.lineVertices.at(index);
        const QPointF previousAnchor = toPreview(previousVertex.anchor);
        const QPointF currentAnchor = toPreview(currentVertex.anchor);
        const QPointF cp1 = toPreview(previousVertex.outgoingControl.value_or(previousVertex.anchor));
        const QPointF cp2 = toPreview(currentVertex.incomingControl.value_or(currentVertex.anchor));
        QPainterPath segmentPath;
        segmentPath.moveTo(previousAnchor);
        if (previousVertex.outgoingControl.has_value() || currentVertex.incomingControl.has_value()) {
            segmentPath.cubicTo(cp1, cp2, currentAnchor);
        } else {
            segmentPath.lineTo(currentAnchor);
        }
        pathDistance += segmentPath.length();
        appendVertex(currentVertex, currentAnchor, pathDistance);
    }

    if (feature.closed && feature.lineVertices.size() >= 2) {
        const MapGeometryFeature::TH2LineVertex &lastVertex = feature.lineVertices.last();
        const MapGeometryFeature::TH2LineVertex &firstVertex = feature.lineVertices.first();
        const QPointF lastAnchor = toPreview(lastVertex.anchor);
        const QPointF firstAnchor = toPreview(firstVertex.anchor);
        const QPointF cp1 = toPreview(lastVertex.outgoingControl.value_or(lastVertex.anchor));
        const QPointF cp2 = toPreview(firstVertex.incomingControl.value_or(firstVertex.anchor));
        QPainterPath segmentPath;
        segmentPath.moveTo(lastAnchor);
        if (lastVertex.outgoingControl.has_value() || firstVertex.incomingControl.has_value()) {
            segmentPath.cubicTo(cp1, cp2, firstAnchor);
        } else {
            segmentPath.lineTo(firstAnchor);
        }
        pathDistance += segmentPath.length();
        appendVertex(firstVertex, firstAnchor, pathDistance);
    }

    return vertices;
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

qreal labelTextRotationForPointOrientation(qreal orientationDegrees)
{
    // Point orientation uses 0=north/up; text's unrotated baseline points east/right.
    return normalizedSceneOrientationDegrees(orientationDegrees - 90.0);
}

std::optional<qreal> linePointNumericOptionValue(const TherionParsedLine &parsedLine, const QStringList &names)
{
    std::optional<qreal> value;
    for (const QString &name : names) {
        const QString valueToken = commandOptionValue(parsedLine.tokens, name).trimmed();
        if (valueToken.isEmpty()) {
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

std::optional<qreal> standaloneDirectiveNumericValue(const TherionParsedLine &parsedLine)
{
    if (parsedLine.tokens.size() < 2) {
        return std::nullopt;
    }

    bool ok = false;
    const qreal parsedValue = parsedLine.tokens.at(1).trimmed().toDouble(&ok);
    if (!ok) {
        return std::nullopt;
    }
    return parsedValue;
}

struct ParsedLinePointOptionFlags
{
    bool parsedOrientation = false;
    bool parsedLeftSize = false;
};

bool isOrientationStandaloneDirective(const QString &directive)
{
    const QString normalized = directive.trimmed().toLower();
    return normalized == QStringLiteral("orientation")
        || normalized == QStringLiteral("-orientation")
        || normalized == QStringLiteral("orient")
        || normalized == QStringLiteral("-orient");
}

bool isLeftSizeStandaloneDirective(const QString &directive)
{
    const QString normalized = directive.trimmed().toLower();
    return normalized == QStringLiteral("l-size")
        || normalized == QStringLiteral("-l-size")
        || normalized == QStringLiteral("size")
        || normalized == QStringLiteral("-size");
}

ParsedLinePointOptionFlags applyLinePointOptionsFromLine(const TherionParsedLine &parsedLine, MapGeometryFeature *feature)
{
    ParsedLinePointOptionFlags flags;
    if (feature == nullptr || feature->lineVertices.isEmpty()) {
        return flags;
    }

    MapGeometryFeature::TH2LineVertex &vertex = feature->lineVertices.last();
    std::optional<qreal> orientation =
        linePointNumericOptionValue(parsedLine,
                                    QStringList{QStringLiteral("-orientation"),
                                                QStringLiteral("orientation"),
                                                QStringLiteral("-orient"),
                                                QStringLiteral("orient")});
    if (!orientation.has_value() && isOrientationStandaloneDirective(parsedLine.directive)) {
        orientation = standaloneDirectiveNumericValue(parsedLine);
    }
    if (orientation.has_value()) {
        flags.parsedOrientation = true;
        vertex.orientationDegrees = normalizedSceneOrientationDegrees(orientation.value());
    }

    if (feature->label.trimmed().toLower() == QStringLiteral("slope")) {
        std::optional<qreal> leftSize =
            linePointNumericOptionValue(parsedLine,
                                        QStringList{QStringLiteral("-size"),
                                                    QStringLiteral("size"),
                                                    QStringLiteral("-l-size"),
                                                    QStringLiteral("l-size")});
        if (!leftSize.has_value() && isLeftSizeStandaloneDirective(parsedLine.directive)) {
            leftSize = standaloneDirectiveNumericValue(parsedLine);
        }
        if (leftSize.has_value() && leftSize.value() > 0.0) {
            flags.parsedLeftSize = true;
            vertex.leftSize = leftSize.value();
        }
    }
    return flags;
}

MapGeometryAreaPlace areaPlaceFromToken(const QString &token)
{
    const QString normalized = token.trimmed().toLower();
    if (normalized == QStringLiteral("bottom")) {
        return MapGeometryAreaPlace::Bottom;
    }
    if (normalized == QStringLiteral("top")) {
        return MapGeometryAreaPlace::Top;
    }
    return MapGeometryAreaPlace::Default;
}

QString optionValueForFieldName(const QHash<QString, QString> &values, const QString &fieldName)
{
    const QString normalizedFieldName = normalizedCommandOptionName(fieldName);
    return normalizedFieldName.isEmpty() ? QString() : values.value(normalizedFieldName).trimmed();
}

QString geometryKindLabel(MapGeometryFeature::Kind kind)
{
    switch (kind) {
    case MapGeometryFeature::Kind::Point:
        return QStringLiteral("point");
    case MapGeometryFeature::Kind::Line:
        return QStringLiteral("line");
    case MapGeometryFeature::Kind::Area:
        return QStringLiteral("area");
    }

    return QStringLiteral("geometry");
}

QString geometryTooltipForFeature(const MapGeometryFeature &feature)
{
    QStringList lines;
    const QString kindLabel = geometryKindLabel(feature.kind);
    const QString typeLabel = feature.label.trimmed();
    lines.append(typeLabel.isEmpty() ? kindLabel : QStringLiteral("%1 %2").arg(kindLabel, typeLabel));

    QStringList detailParts;
    const QString objectId = optionValueForFieldName(feature.optionValues, QStringLiteral("id"));
    if (!objectId.isEmpty()) {
        detailParts.append(QStringLiteral("-id %1").arg(objectId));
    }
    const QString subtype = feature.subtype.trimmed();
    if (!subtype.isEmpty()) {
        detailParts.append(QStringLiteral("-subtype %1").arg(subtype));
    }
    if (!detailParts.isEmpty()) {
        lines.append(detailParts.join(QStringLiteral(" ")));
    }
    if (feature.lineNumber > 0) {
        lines.append(QObject::tr("Source line %1").arg(feature.lineNumber));
    }

    return lines.join(QLatin1Char('\n'));
}

QString pointTypeTokenFromLine(const TherionParsedLine &parsedLine)
{
    if (parsedLine.directive != QStringLiteral("point")) {
        return QString();
    }

    int numericCoordinateTokens = 0;
    for (int index = 1; index < parsedLine.tokens.size(); ++index) {
        const QString token = parsedLine.tokens.at(index).trimmed();
        if (token.isEmpty()) {
            continue;
        }
        if (TherionTokenRules::tokenStartsOption(token)) {
            break;
        }
        if (TherionTokenRules::isNumericToken(token)) {
            ++numericCoordinateTokens;
            continue;
        }
        if (numericCoordinateTokens < 2) {
            continue;
        }
        return token.toLower();
    }

    return QString();
}

QString geometryFeatureTypePart(const QString &typeToken)
{
    return typeToken.section(QLatin1Char(':'), 0, 0).trimmed().toLower();
}

QString geometryFeatureInlineSubtypePart(const QString &typeToken)
{
    return typeToken.section(QLatin1Char(':'), 1).trimmed().toLower();
}

QString geometryFeatureSubtypeFromParsedLine(const TherionParsedLine &parsedLine, const QString &typeToken)
{
    const QString optionSubtype = commandOptionValue(parsedLine.tokens, QStringLiteral("-subtype")).trimmed().toLower();
    if (!optionSubtype.isEmpty()) {
        return optionSubtype;
    }

    return geometryFeatureInlineSubtypePart(typeToken);
}

QString stationPointNameFromLine(const TherionParsedLine &parsedLine)
{
    const QString optionName = commandOptionValue(parsedLine.tokens, QStringLiteral("-name")).trimmed();
    if (!optionName.isEmpty()) {
        return optionName;
    }

    const QString typeToken = pointTypeTokenFromLine(parsedLine);
    if (typeToken != QStringLiteral("station")) {
        return QString();
    }

    bool sawTypeToken = false;
    for (int index = 1; index < parsedLine.tokens.size(); ++index) {
        const QString token = parsedLine.tokens.at(index).trimmed();
        if (token.isEmpty()) {
            continue;
        }
        if (TherionTokenRules::tokenStartsOption(token)) {
            break;
        }
        if (!sawTypeToken) {
            if (token.toLower() == QStringLiteral("station")) {
                sawTypeToken = true;
            }
            continue;
        }
        if (!TherionTokenRules::isNumericToken(token)) {
            return token;
        }
    }
    return QString();
}

QString therionLabelTextToHtml(const QString &text)
{
    QString html;
    bool styleSpanOpen = false;
    int rtlSpanDepth = 0;
    QString alignment;
    const bool hasLineBreak = QRegularExpression(QStringLiteral(R"(<\s*br\s*>)"),
                                                 QRegularExpression::CaseInsensitiveOption).match(text).hasMatch();

    auto closeStyleSpan = [&]() {
        if (styleSpanOpen) {
            html += QStringLiteral("</span>");
            styleSpanOpen = false;
        }
    };
    auto openStyleSpan = [&](const QString &style) {
        closeStyleSpan();
        if (!style.isEmpty()) {
            html += QStringLiteral("<span style=\"%1\">").arg(style.toHtmlEscaped());
            styleSpanOpen = true;
        }
    };

    for (int index = 0; index < text.size();) {
        if (text.at(index) != QLatin1Char('<')) {
            html += QString(text.at(index)).toHtmlEscaped();
            ++index;
            continue;
        }

        const int endIndex = text.indexOf(QLatin1Char('>'), index + 1);
        if (endIndex < 0) {
            html += QString(text.at(index)).toHtmlEscaped();
            ++index;
            continue;
        }

        const QString tag = text.mid(index + 1, endIndex - index - 1).trimmed();
        const QString normalized = tag.toLower();
        bool handled = true;
        if (normalized == QStringLiteral("br")) {
            html += QStringLiteral("<br/>");
        } else if (normalized == QStringLiteral("thsp")) {
            html += QStringLiteral("&#8201;");
        } else if (normalized == QStringLiteral("rm")) {
            closeStyleSpan();
        } else if (normalized == QStringLiteral("bf")) {
            openStyleSpan(QStringLiteral("font-weight:700;"));
        } else if (normalized == QStringLiteral("it")) {
            openStyleSpan(QStringLiteral("font-style:italic;"));
        } else if (normalized == QStringLiteral("ss")) {
            openStyleSpan(QStringLiteral("font-family:sans-serif;"));
        } else if (normalized == QStringLiteral("si")) {
            openStyleSpan(QStringLiteral("font-family:sans-serif; font-style:italic;"));
        } else if (normalized.startsWith(QStringLiteral("size:"))) {
            const QString sizeValue = normalized.mid(5).trimmed();
            if (sizeValue == QStringLiteral("xs")) {
                openStyleSpan(QStringLiteral("font-size:70%;"));
            } else if (sizeValue == QStringLiteral("s")) {
                openStyleSpan(QStringLiteral("font-size:85%;"));
            } else if (sizeValue == QStringLiteral("m")) {
                openStyleSpan(QStringLiteral("font-size:100%;"));
            } else if (sizeValue == QStringLiteral("l")) {
                openStyleSpan(QStringLiteral("font-size:120%;"));
            } else if (sizeValue == QStringLiteral("xl")) {
                openStyleSpan(QStringLiteral("font-size:145%;"));
            } else if (sizeValue.endsWith(QLatin1Char('%'))) {
                bool ok = false;
                const int percent = sizeValue.left(sizeValue.size() - 1).toInt(&ok);
                if (ok && percent >= 1 && percent <= 999) {
                    openStyleSpan(QStringLiteral("font-size:%1%;").arg(percent));
                }
            } else {
                bool ok = false;
                const int points = sizeValue.toInt(&ok);
                if (ok && points >= 1 && points <= 127) {
                    openStyleSpan(QStringLiteral("font-size:%1pt;").arg(points));
                }
            }
        } else if (normalized == QStringLiteral("rtl")) {
            html += QStringLiteral("<span dir=\"rtl\">");
            ++rtlSpanDepth;
        } else if (normalized == QStringLiteral("/rtl")) {
            if (rtlSpanDepth > 0) {
                closeStyleSpan();
                html += QStringLiteral("</span>");
                --rtlSpanDepth;
            }
        } else if (normalized == QStringLiteral("left")
                   || normalized == QStringLiteral("right")
                   || normalized == QStringLiteral("center")
                   || normalized == QStringLiteral("centre")) {
            if (hasLineBreak) {
                alignment = normalized == QStringLiteral("centre") ? QStringLiteral("center") : normalized;
            }
        } else if (normalized.startsWith(QStringLiteral("lang:"))) {
            // Accepted Therion label metadata; language choice is handled before rendering.
        } else {
            handled = false;
        }

        if (!handled) {
            html += text.mid(index, endIndex - index + 1).toHtmlEscaped();
        }
        index = endIndex + 1;
    }

    closeStyleSpan();
    while (rtlSpanDepth > 0) {
        html += QStringLiteral("</span>");
        --rtlSpanDepth;
    }

    if (!alignment.isEmpty()) {
        return QStringLiteral("<div align=\"%1\">%2</div>").arg(alignment, html);
    }
    return QStringLiteral("<div>%1</div>").arg(html);
}

bool therionLabelTextNeedsRichRendering(const QString &text)
{
    static const QRegularExpression markupPattern(
        QStringLiteral(R"(<\s*(?:br|thsp|rm|it|bf|ss|si|rtl|/rtl|left|right|center|centre|lang:[^>]+|size:[^>]+)\s*>)"),
        QRegularExpression::CaseInsensitiveOption);
    return markupPattern.match(text).hasMatch();
}

QStaticText createPointLabelStaticText(const QString &text, bool useTherionFormatting, const QFont &font)
{
    QStaticText staticText;
    if (!useTherionFormatting || !therionLabelTextNeedsRichRendering(text)) {
        staticText.setText(text);
        staticText.setTextFormat(Qt::PlainText);
    } else {
        staticText.setText(therionLabelTextToHtml(text));
        staticText.setTextFormat(Qt::RichText);
    }
    staticText.setPerformanceHint(QStaticText::AggressiveCaching);
    staticText.prepare(QTransform(), font);
    return staticText;
}

QString lineLabelPathText(QString text)
{
    static const QRegularExpression breakPattern(QStringLiteral(R"(<\s*br\s*/?\s*>)"),
                                                 QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression thinSpacePattern(QStringLiteral(R"(<\s*thsp\s*/?\s*>)"),
                                                     QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression ignoredLabelTagPattern(
        QStringLiteral(R"(<\s*/?\s*(?:rm|it|bf|ss|si|rtl|left|right|center|centre)\s*>)"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression metadataLabelTagPattern(
        QStringLiteral(R"(<\s*(?:lang|size):[^>]*>)"),
        QRegularExpression::CaseInsensitiveOption);
    text.replace(breakPattern, QStringLiteral(" "));
    text.replace(thinSpacePattern, QString(QChar(0x2009)));
    text.replace(ignoredLabelTagPattern, QString());
    text.replace(metadataLabelTagPattern, QString());
    return text.trimmed();
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
        if (token.isEmpty()
            || TherionTokenRules::isNumericToken(token)
            || TherionTokenRules::tokenStartsOption(token)) {
            continue;
        }
        identifiers->append(token);
    }
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
        "<p>Line-anchor editing shortcut: <code>Delete</code>/<code>Backspace</code> removes a selected middle anchor.</p>"
        "<p>Background image layers are managed from the Map sidebar, including layer order, visibility, position, opacity, and gamma.</p>"
        "<p>When present, <code>##XTHERION## xth_me_image_insert</code> metadata is used to auto-load referenced background images.</p>"
        "<p>Drag parsed geometry handles to rewrite source coordinates. Select a draft item to move or toggle it.</p>");
}

MapGeometryItemGroupRemovalResult removeMapGeometryItemGroupForLine(QGraphicsScene *scene,
                                                                    int lineNumber,
                                                                    QHash<int, QGraphicsItem *> *mapItemsByLine,
                                                                    QHash<QString, QGraphicsItem *> *mapVertexItemsByKey)
{
    MapGeometryItemGroupRemovalResult result;
    if (scene == nullptr || lineNumber <= 0) {
        return result;
    }

    auto isGeometryItemForLine = [lineNumber](const QGraphicsItem *item) {
        return item != nullptr
            && item->data(kMapItemRole).toInt() == kMapItemGeometryValue
            && item->data(kMapSceneLineNumberRole).toInt() == lineNumber;
    };

    QList<QGraphicsItem *> itemsToRemove;
    for (QGraphicsItem *item : scene->items()) {
        if (isGeometryItemForLine(item)) {
            itemsToRemove.append(item);
        }
    }

    if (mapItemsByLine != nullptr && mapItemsByLine->contains(lineNumber)) {
        mapItemsByLine->remove(lineNumber);
        result.removedPrimaryItem = true;
    }

    if (mapVertexItemsByKey != nullptr) {
        auto it = mapVertexItemsByKey->begin();
        while (it != mapVertexItemsByKey->end()) {
            if (isGeometryItemForLine(it.value())) {
                it = mapVertexItemsByKey->erase(it);
                ++result.removedVertexIndexEntries;
            } else {
                ++it;
            }
        }
    }

    for (QGraphicsItem *item : itemsToRemove) {
        scene->removeItem(item);
        delete item;
        ++result.removedItems;
    }

    return result;
}

MapGeometryItemGroupRenderResult renderMapGeometryItemGroupForFeature(
    QGraphicsScene *scene,
    const MapGeometryFeature &feature,
    const QRectF &sourceBounds,
    QHash<int, QGraphicsItem *> *mapItemsByLine,
    QHash<QString, QGraphicsItem *> *mapVertexItemsByKey,
    const std::function<void(int, const QPointF &, const QPointF &)> &recordPointGeometryMove,
    const std::function<void(int, const QString &, int, const QPointF &, const QPointF &)> &recordLineAreaVertexMove,
    const std::function<void(int, qreal)> &recordPointOrientationHandleChange,
    const std::function<void(int, int, qreal, qreal)> &recordLinePointLeftHandleChange,
    const MapEditorObjectStyleCatalog &styleCatalog)
{
    MapGeometryItemGroupRenderResult result;
    if (scene == nullptr || feature.lineNumber <= 0) {
        return result;
    }

    // Reuse the established feature branch directly in the target scene.  Keeping
    // the per-feature indexes local prevents a partial refresh from resetting the
    // indexes owned by the complete workspace scene.
    const QList<QGraphicsItem *> existingItems = scene->items();
    const QSet<QGraphicsItem *> existingItemSet(existingItems.cbegin(), existingItems.cend());
    QHash<int, QGraphicsItem *> featureItemsByLine;
    QHash<QString, QGraphicsItem *> featureVertexItemsByKey;
    renderMapWorkspaceScene(scene,
                            QString(),
                            {},
                            QVector<MapGeometryFeature>{feature},
                            sourceBounds.isValid() ? std::optional<QRectF>(sourceBounds) : std::nullopt,
                            false,
                            &featureItemsByLine,
                            &featureVertexItemsByKey,
                            {},
                            {},
                            recordPointGeometryMove,
                            recordLineAreaVertexMove,
                            recordPointOrientationHandleChange,
                            recordLinePointLeftHandleChange,
                            styleCatalog,
                            false);

    auto isGeometryItemForFeature = [&feature](const QGraphicsItem *item) {
        return item != nullptr
            && item->data(kMapItemRole).toInt() == kMapItemGeometryValue
            && item->data(kMapSceneLineNumberRole).toInt() == feature.lineNumber;
    };

    QList<QGraphicsItem *> itemsToRemove;
    for (QGraphicsItem *item : scene->items()) {
        if (item == nullptr || existingItemSet.contains(item)) {
            continue;
        }
        if (isGeometryItemForFeature(item)) {
            ++result.addedItems;
        } else {
            // The workspace renderer adds its canvas frame. It is not part of a
            // geometry item group and must not accumulate on partial refresh.
            itemsToRemove.append(item);
        }
    }

    for (QGraphicsItem *item : itemsToRemove) {
        scene->removeItem(item);
        delete item;
    }

    if (mapItemsByLine != nullptr) {
        QGraphicsItem *primaryItem = featureItemsByLine.value(feature.lineNumber, nullptr);
        if (isGeometryItemForFeature(primaryItem)) {
            mapItemsByLine->insert(feature.lineNumber, primaryItem);
            result.addedPrimaryItem = true;
        }
    }

    if (mapVertexItemsByKey != nullptr) {
        for (auto it = featureVertexItemsByKey.cbegin(); it != featureVertexItemsByKey.cend(); ++it) {
            if (!isGeometryItemForFeature(it.value())) {
                continue;
            }
            mapVertexItemsByKey->insert(it.key(), it.value());
            ++result.addedVertexIndexEntries;
        }
    }

    return result;
}

QString mapEntryCategoryForLine(const TherionParsedLine &parsedLine)
{
    if (parsedLine.tokens.isEmpty()) {
        return QString();
    }

    const QString directive = parsedLine.tokens.first().toLower();

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
    if (directive == QStringLiteral("point") && pointTypeTokenFromLine(parsedLine) == QStringLiteral("station")) {
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
        if (directive == QStringLiteral("point")) {
            const QString pointType = pointTypeTokenFromLine(parsedLine);
            if (geometryFeatureTypePart(pointType) == QStringLiteral("station")) {
                const QString stationName = stationPointNameFromLine(parsedLine);
                return stationName.isEmpty() ? geometryFeatureTypePart(pointType) : stationName;
            }
            if (!pointType.isEmpty()) {
                return geometryFeatureTypePart(pointType);
            }
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

    QString subtitle = remainder.join(QStringLiteral(" "));
    if (parsedLine.tokens.first().toLower() == QStringLiteral("line")) {
        const bool closed = commandOptionToggleValue(parsedLine.tokens, QStringLiteral("-close")).value_or(false);
        const bool reversed = commandOptionToggleValue(parsedLine.tokens, QStringLiteral("-reverse")).value_or(false);
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

QVector<MapSceneEntry> collectMapSceneEntries(const QVector<TherionSourceLogicalCommand> &commands)
{
    QVector<TherionParsedLine> parsedLines;
    parsedLines.reserve(commands.size());
    for (const TherionSourceLogicalCommand &command : commands) {
        parsedLines.append(command.parsed);
    }
    return collectMapSceneEntries(parsedLines);
}

void renderMapWorkspaceScene(QGraphicsScene *scene,
                             const QString &documentPath,
                             const QVector<MapSceneEntry> &entries,
                             const QVector<MapGeometryFeature> &geometryFeatures,
                             const std::optional<QRectF> &sourceBoundsOverride,
                             bool showEmptyDocumentGuides,
                             QHash<int, QGraphicsItem *> *mapItemsByLine,
                             QHash<QString, QGraphicsItem *> *mapVertexItemsByKey,
                             const std::function<void(int, const QPointF &, const QPointF &)> &recordCardMove,
                             const std::function<void(int, bool, bool)> &recordCardVisibility,
                             const std::function<void(int, const QPointF &, const QPointF &)> &recordPointGeometryMove,
                             const std::function<void(int, const QString &, int, const QPointF &, const QPointF &)> &recordLineAreaVertexMove,
                             const std::function<void(int, qreal)> &recordPointOrientationHandleChange,
                             const std::function<void(int, int, qreal, qreal)> &recordLinePointLeftHandleChange,
                             const MapEditorObjectStyleCatalog &styleCatalog,
                             bool renderCanvasFrame)
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
    if (mapVertexItemsByKey != nullptr) {
        mapVertexItemsByKey->clear();
    }

    const auto registerVertexItem = [mapVertexItemsByKey](MapEditableGeometryVertexItem *item) {
        if (mapVertexItemsByKey == nullptr || item == nullptr) {
            return;
        }
        const QString key = QStringLiteral("%1:%2:%3")
            .arg(item->lineNumber())
            .arg(item->vertexIndex())
            .arg(item->geometryKind().trimmed().toLower());
        mapVertexItemsByKey->insert(key, item);
    };

    const MapCanvasTheme canvasTheme = mapCanvasThemeForScene(scene);
    const QRectF sceneFrame = mapEditorCanvasSceneFrame();
    const QRectF geometryCanvas = sceneFrame.adjusted(kMapEditorCanvasFrameInset,
                                                       kMapEditorCanvasFrameInset,
                                                       -kMapEditorCanvasFrameInset,
                                                       -kMapEditorCanvasFrameInset);
    if (renderCanvasFrame) {
        scene->setSceneRect(sceneFrame);
    }
    if (renderCanvasFrame && (showEmptyDocumentGuides || !geometryFeatures.isEmpty())) {
        QGraphicsRectItem *canvasItem =
            makeMouseTransparent(scene->addRect(geometryCanvas, QPen(canvasTheme.canvasBorder, 1.2), QBrush(canvasTheme.canvasFill)));
        if (canvasItem != nullptr && geometryFeatures.isEmpty()) {
            canvasItem->setData(kMapSceneEmptyDocumentGuideRole, true);
        }
    }

    const QRectF previewBounds = mapEditorCanvasPreviewBounds();
    const QRectF sourceBounds = (sourceBoundsOverride.has_value() && sourceBoundsOverride->isValid())
        ? sourceBoundsOverride.value()
        : geometryBoundsForFeatures(geometryFeatures);
    const qreal mapScale = sceneCoordsScaleFactor(sourceBounds, previewBounds);
    const QHash<int, QPainterPath> scrapClipPaths =
        scrapClipPathsForFeatures(geometryFeatures, sourceBounds, previewBounds);
    const qreal vertexRadius = 3.8;
    auto markGeometryItem = [](QGraphicsItem *item) {
        if (item != nullptr) {
            item->setData(kMapItemRole, kMapItemGeometryValue);
        }
    };

    if (renderCanvasFrame && geometryFeatures.isEmpty() && showEmptyDocumentGuides) {
        auto *emptyGeometryItem = makeMouseTransparent(scene->addText(QObject::tr("No parseable point, line, or area geometry was found in this document yet."), QFont(QStringLiteral("Menlo"), 11)));
        emptyGeometryItem->setData(kMapSceneEmptyDocumentGuideRole, true);
        emptyGeometryItem->setDefaultTextColor(canvasTheme.mutedText);
        emptyGeometryItem->setPos(previewBounds.left() + 16.0, previewBounds.top() + 16.0);
    } else {
        for (const MapGeometryFeature &feature : geometryFeatures) {
            switch (feature.kind) {
            case MapGeometryFeature::Kind::Point: {
                if (!feature.hasAnchor) {
                    break;
                }
                const QString featureTooltip = geometryTooltipForFeature(feature);

                const MapEditorResolvedPointStyle pointStyle = resolveMapEditorPointStyle(styleCatalog,
                                                                                           feature.stationPoint ? QStringLiteral("station") : feature.label,
                                                                                           feature.subtype);
                const qreal pointSize = qBound(4.0, pointStyle.size, 48.0);
                const QColor pointFillColor = pointStyle.fillColor.value_or(canvasTheme.pointHandleFill);
                const QColor pointStrokeColor = pointStyle.strokeColor.value_or(canvasTheme.pointHandleStroke);
                const QPointF previewPoint = mapGeometryPointToPreview(feature.anchor, sourceBounds, previewBounds);
                auto *pointItem = new MapEditablePointItem(feature.lineNumber, feature.anchor, sourceBounds, previewBounds);
                pointItem->setRect(QRectF(-pointSize / 2.0, -pointSize / 2.0, pointSize, pointSize));
                pointItem->setSymbol(pointStyle.symbol);
                pointItem->setSymbolParts(pointStyle.symbolParts);
                pointItem->setPointAlignAffectsSymbol(!pointStyle.labelField.has_value());
                pointItem->setPointAlign(optionValueForFieldName(feature.optionValues, QStringLiteral("align")));
                if (feature.orientationSupported && explicitOrientation(feature.orientationDegrees)) {
                    pointItem->setSymbolRotationDegrees(feature.orientationDegrees.value());
                }
                pointItem->setPen(cosmeticPen(pointStrokeColor, qBound(0.6, pointStyle.outlineWidth, 8.0)));
                pointItem->setBrush(QBrush(pointFillColor));
                pointItem->setMoveCommittedCallback(recordPointGeometryMove);
                pointItem->setToolTip(featureTooltip);
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

                if (pointStyle.labelField.has_value()) {
                    const QString labelText = optionValueForFieldName(feature.optionValues, pointStyle.labelField.value());
                    if (!labelText.isEmpty()) {
                        const bool labelTextField = normalizedCommandOptionName(pointStyle.labelField.value()) == QStringLiteral("text");
                        QFont labelFont(QStringLiteral("Menlo"));
                        labelFont.setPointSizeF(pointStyle.labelFontSize.value_or(10.0));
                        if (feature.stationPoint) {
                            labelFont.setBold(true);
                        }
                        pointItem->setLabel(createPointLabelStaticText(labelText,
                                                                       labelTextField,
                                                                       labelFont),
                                            labelFont,
                                            canvasTheme.labelText);
                        if (feature.stationPoint) {
                            pointItem->setStationLabelAutoVisibilityEnabled(true);
                        }
                        if (labelTextField
                            && pointStyle.labelOrientation == MapEditorPointLabelOrientationMode::Orientation
                            && feature.orientationSupported
                            && explicitOrientation(feature.orientationDegrees)) {
                            pointItem->setLabelRotationDegrees(
                                labelTextRotationForPointOrientation(feature.orientationDegrees.value()));
                        }
                    }
                }
                break;
            }
            case MapGeometryFeature::Kind::Line: {
                if (feature.lineVertices.size() < 2) {
                    break;
                }
                const QString featureTooltip = geometryTooltipForFeature(feature);

                const MapEditorResolvedLineStyle lineStyle = resolveMapEditorLineStyle(styleCatalog,
                                                                                       feature.label,
                                                                                       feature.subtype);
                const qreal thickLineWidth = qBound(0.8, lineStyle.strokeWidth, 24.0);
                const QPainterPath path = linePathForFeature(feature, sourceBounds, previewBounds);
                const QVector<StyledLinePath> styledSegmentPaths = styledLinePathsForFeature(feature,
                                                                                             sourceBounds,
                                                                                             previewBounds);
                const bool renderSegmentStyles = std::any_of(styledSegmentPaths.cbegin(),
                                                             styledSegmentPaths.cend(),
                                                             [&](const StyledLinePath &styledPath) {
                    return styledPath.subtype != feature.subtype.trimmed().toLower();
                });
                QColor geometryStroke = lineStyle.strokeColor.value_or(canvasTheme.geometryStroke);
                QColor baseLineStroke = geometryStroke;
                if (!lineStyle.strokeVisible || renderSegmentStyles) {
                    baseLineStroke.setAlpha(0);
                }

                if (feature.closed && feature.lineVertices.size() >= 2) {
                    if (const std::optional<QColor> closedFillColor = closedLineFillColor(lineStyle, canvasTheme)) {
                        QPainterPath closedFillPath = path;
                        closedFillPath.closeSubpath();
                        closedFillPath.setFillRule(Qt::OddEvenFill);
                        auto *closedFillItem = new MapZoomAwarePathItem(closedFillPath,
                                                                        QPen(Qt::NoPen),
                                                                        QBrush(closedFillColor.value()));
                        scene->addItem(closedFillItem);
                        closedFillItem->setFlag(QGraphicsItem::ItemIsSelectable, true);
                        closedFillItem->setZValue(2.49);
                        closedFillItem->setToolTip(featureTooltip);
                        markGeometryItem(closedFillItem);
                        closedFillItem->setData(kMapSceneLineNumberRole, feature.lineNumber);
                    }
                }

                auto *lineItem = new MapZoomAwarePathItem(path,
                                                          styledGeometricPen(baseLineStroke,
                                                                             thickLineWidth,
                                                                             lineStyle.strokeVisible ? lineStyle.penStyle : Qt::SolidLine,
                                                                             lineStyle.strokeVisible ? lineStyle.dashPattern : QVector<qreal>{},
                                                                             Qt::RoundCap,
                                                                             Qt::RoundJoin));
                scene->addItem(lineItem);
                lineItem->setFlag(QGraphicsItem::ItemIsSelectable, true);
                lineItem->setZValue(2.5);
                lineItem->setToolTip(featureTooltip);
                markGeometryItem(lineItem);
                lineItem->setData(kMapSceneLineNumberRole, feature.lineNumber);
                auto styledLineItems = std::make_shared<QVector<StyledPathItemBinding>>();
                auto styledDecorationItems = std::make_shared<QVector<StyledLineDecorationBinding>>();
                auto styledGuideItems = std::make_shared<QVector<StyledPathItemBinding>>();
                if (renderSegmentStyles) {
                    for (int styledPathIndex = 0; styledPathIndex < styledSegmentPaths.size(); ++styledPathIndex) {
                        const StyledLinePath &styledPath = styledSegmentPaths.at(styledPathIndex);
                        const MapEditorResolvedLineStyle segmentStyle =
                            resolveMapEditorLineStyle(styleCatalog, feature.label, styledPath.subtype);
                        QColor segmentStroke = segmentStyle.strokeColor.value_or(canvasTheme.geometryStroke);
                        if (!segmentStyle.strokeVisible) {
                            segmentStroke.setAlpha(0);
                        }
                        auto *segmentItem = new MapZoomAwarePathItem(styledPath.path,
                                                                     styledGeometricPen(segmentStroke,
                                                                                        qBound(0.8, segmentStyle.strokeWidth, 24.0),
                                                                                        segmentStyle.strokeVisible ? segmentStyle.penStyle : Qt::SolidLine,
                                                                                        segmentStyle.strokeVisible ? segmentStyle.dashPattern : QVector<qreal>{},
                                                                                        Qt::RoundCap,
                                                                                        Qt::RoundJoin));
                        scene->addItem(segmentItem);
                        segmentItem->setFlag(QGraphicsItem::ItemIsSelectable, true);
                        segmentItem->setZValue(2.5);
                        segmentItem->setToolTip(featureTooltip);
                        markGeometryItem(segmentItem);
                        segmentItem->setData(kMapSceneLineNumberRole, feature.lineNumber);
                        styledLineItems->append(StyledPathItemBinding{styledPathIndex, segmentItem});
                    }
                }
                auto addGuideSpineItem = [&](const QPainterPath &guidePath, qreal referenceStrokeWidth) {
                    QColor guideStroke = canvasTheme.mutedText;
                    guideStroke.setAlpha(210);
                    QVector<qreal> guideDashPattern;
                    guideDashPattern << 2.0 << 2.4;
                    auto *guideItem = new MapZoomAwarePathItem(guidePath,
                                                               styledGeometricPen(guideStroke,
                                                                                  qBound(0.7,
                                                                                         referenceStrokeWidth * 0.34,
                                                                                         2.2),
                                                                                  Qt::DashLine,
                                                                                  guideDashPattern,
                                                                                  Qt::RoundCap,
                                                                                  Qt::RoundJoin));
                    scene->addItem(guideItem);
                    guideItem->setFlag(QGraphicsItem::ItemIsSelectable, true);
                    guideItem->setHoverInteractionOverlayStroke(1.3, 0.15);
                    guideItem->setZValue(2.58);
                    guideItem->setToolTip(featureTooltip);
                    markGeometryItem(guideItem);
                    guideItem->setData(kMapSceneLineNumberRole, feature.lineNumber);
                    guideItem->setData(kMapSceneSelectionSubtypeRole, kMapSceneSelectionSubtypeLineDetail);
                    return guideItem;
                };
                const bool renderLineGuideSpine = lineStyle.guideSpineVisible || !lineStyle.decorations.isEmpty();
                MapEditorLineDecorationItem *lineDecorationItem = nullptr;
                QGraphicsPathItem *lineGuideSpineItem = nullptr;
                if (renderSegmentStyles) {
                    for (int styledPathIndex = 0; styledPathIndex < styledSegmentPaths.size(); ++styledPathIndex) {
                        const StyledLinePath &styledPath = styledSegmentPaths.at(styledPathIndex);
                        const MapEditorResolvedLineStyle segmentStyle =
                            resolveMapEditorLineStyle(styleCatalog, feature.label, styledPath.subtype);
                        if (segmentStyle.decorations.isEmpty()) {
                            continue;
                        }

                        QColor segmentGeometryStroke = segmentStyle.strokeColor.value_or(canvasTheme.geometryStroke);
                        auto *segmentDecorationItem = new MapEditorLineDecorationItem(styledPath.path,
                                                                                      segmentStyle.decorations,
                                                                                      segmentGeometryStroke,
                                                                                      feature.reversed,
                                                                                      feature.lineNumber,
                                                                                      {},
                                                                                      mapScale);
                        scene->addItem(segmentDecorationItem);
                        segmentDecorationItem->setZValue(2.55);
                        markGeometryItem(segmentDecorationItem);
                        segmentDecorationItem->setFlag(QGraphicsItem::ItemIsSelectable, true);
                        segmentDecorationItem->setData(kMapSceneLineNumberRole, feature.lineNumber);
                        segmentDecorationItem->setData(kMapSceneSelectionSubtypeRole, kMapSceneSelectionSubtypeLineDetail);
                        styledDecorationItems->append(StyledLineDecorationBinding{styledPathIndex, segmentDecorationItem});
                    }
                    for (int styledPathIndex = 0; styledPathIndex < styledSegmentPaths.size(); ++styledPathIndex) {
                        const StyledLinePath &styledPath = styledSegmentPaths.at(styledPathIndex);
                        const MapEditorResolvedLineStyle segmentStyle =
                            resolveMapEditorLineStyle(styleCatalog, feature.label, styledPath.subtype);
                        if (!lineSegmentNeedsGuideSpine(segmentStyle, feature.label, styledPath.subtype)) {
                            continue;
                        }

                        styledGuideItems->append(StyledPathItemBinding{
                            styledPathIndex,
                            addGuideSpineItem(styledPath.path, qBound(0.8, segmentStyle.strokeWidth, 24.0))});
                    }
                } else if (!lineStyle.decorations.isEmpty()) {
                    lineDecorationItem = new MapEditorLineDecorationItem(path,
                                                                         lineStyle.decorations,
                                                                         geometryStroke,
                                                                         feature.reversed,
                                                                         feature.lineNumber,
                                                                         lineDecorationVerticesForFeature(feature,
                                                                                                          sourceBounds,
                                                                                                          previewBounds),
                                                                         mapScale);
                    scene->addItem(lineDecorationItem);
                    lineDecorationItem->setZValue(2.55);
                    markGeometryItem(lineDecorationItem);
                    lineDecorationItem->setFlag(QGraphicsItem::ItemIsSelectable, true);
                    lineDecorationItem->setData(kMapSceneLineNumberRole, feature.lineNumber);
                    lineDecorationItem->setData(kMapSceneSelectionSubtypeRole, kMapSceneSelectionSubtypeLineDetail);
                }
                if (renderLineGuideSpine) {
                    lineGuideSpineItem = addGuideSpineItem(path, thickLineWidth);
                }
                const qreal lineDirectionTickLength = qBound(12.0, 18.0 * mapScale, 24.0);
                auto *directionTickItem = new QGraphicsLineItem;
                directionTickItem->setPen(cosmeticPen(mapEditorDirectionTickColor(),
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

                MapPathLabelItem *lineLabelItem = nullptr;
                if (lineStyle.labelField.has_value()) {
                    const QString labelText = optionValueForFieldName(feature.optionValues, lineStyle.labelField.value());
                    if (!labelText.isEmpty()) {
                        QFont labelFont(QStringLiteral("Menlo"), 10);
                        lineLabelItem = new MapPathLabelItem(lineLabelPathText(labelText),
                                                             path,
                                                             labelFont,
                                                             canvasTheme.labelText);
                        scene->addItem(lineLabelItem);
                        lineLabelItem->setZValue(3.1);
                        markGeometryItem(lineLabelItem);
                        makeMouseTransparent(lineLabelItem);
                        lineLabelItem->setData(kMapSceneLineNumberRole, feature.lineNumber);
                    }
                }

                auto anchorItemsByOrder = std::make_shared<QVector<MapEditableGeometryVertexItem *>>(feature.lineVertices.size(), nullptr);
                auto controlItemsBySourceVertex = std::make_shared<QHash<int, MapEditableGeometryVertexItem *>>();
                auto controlConnectors = std::make_shared<QVector<LineControlConnectorBinding>>();

                for (int vertexIndex = 0; vertexIndex < feature.lineVertices.size(); ++vertexIndex) {
                    const MapGeometryFeature::TH2LineVertex &vertex = feature.lineVertices.at(vertexIndex);
                    if (linePointRowsShouldHighlightVertexMetadata(vertex.standaloneOptionRows)) {
                        const QPointF markerCenter = mapGeometryPointToPreview(vertex.anchor, sourceBounds, previewBounds);
                        const QRectF markerRect(-6.0, -6.0, 12.0, 12.0);
                        auto *metadataShadow = new QGraphicsEllipseItem(markerRect);
                        QColor shadowColor(QStringLiteral("#1f2937"));
                        shadowColor.setAlpha(75);
                        metadataShadow->setPen(cosmeticPen(shadowColor, 1.4));
                        metadataShadow->setBrush(Qt::NoBrush);
                        metadataShadow->setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
                        metadataShadow->setAcceptedMouseButtons(Qt::NoButton);
                        metadataShadow->setPos(markerCenter);
                        metadataShadow->setZValue(3.86);
                        markGeometryItem(metadataShadow);
                        makeMouseTransparent(metadataShadow);
                        metadataShadow->setData(kMapSceneLineNumberRole, feature.lineNumber);
                        scene->addItem(metadataShadow);

                        auto *metadataRing = new QGraphicsEllipseItem(markerRect);
                        QColor ringColor(QStringLiteral("#f59e0b"));
                        ringColor.setAlpha(115);
                        metadataRing->setPen(cosmeticPen(ringColor, 0.8));
                        metadataRing->setBrush(Qt::NoBrush);
                        metadataRing->setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
                        metadataRing->setAcceptedMouseButtons(Qt::NoButton);
                        metadataRing->setPos(markerCenter);
                        metadataRing->setZValue(3.87);
                        markGeometryItem(metadataRing);
                        makeMouseTransparent(metadataRing);
                        metadataRing->setData(kMapSceneLineNumberRole, feature.lineNumber);
                        scene->addItem(metadataRing);
                    }

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
                    vertexItem->setStandaloneOptionRows(vertex.standaloneOptionRows);
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
                    registerVertexItem(vertexItem);
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

                const qreal controlRadius = 2.8;
                const qreal controlConnectorWidth = qBound<qreal>(1.4, 1.8 * mapScale, 2.4);
                for (int segmentIndex = 1; segmentIndex < feature.lineVertices.size(); ++segmentIndex) {
                    const MapGeometryFeature::TH2LineVertex &previousVertex = feature.lineVertices.at(segmentIndex - 1);
                    const MapGeometryFeature::TH2LineVertex &currentVertex = feature.lineVertices.at(segmentIndex);

                    if (previousVertex.outgoingControl.has_value() && previousVertex.outgoingSourceVertexIndex >= 0) {
                        const QPointF anchorPreview = mapGeometryPointToPreview(previousVertex.anchor, sourceBounds, previewBounds);
                        const QPointF controlPreview = mapGeometryPointToPreview(previousVertex.outgoingControl.value(), sourceBounds, previewBounds);
                        auto *connector = scene->addLine(QLineF(anchorPreview, controlPreview),
                                                         cosmeticPen(canvasTheme.controlConnector, controlConnectorWidth, Qt::DashLine, Qt::RoundCap));
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
                        registerVertexItem(controlItem);
                        controlItemsBySourceVertex->insert(previousVertex.outgoingSourceVertexIndex, controlItem);
                    }

                    if (currentVertex.incomingControl.has_value() && currentVertex.incomingSourceVertexIndex >= 0) {
                        const QPointF anchorPreview = mapGeometryPointToPreview(currentVertex.anchor, sourceBounds, previewBounds);
                        const QPointF controlPreview = mapGeometryPointToPreview(currentVertex.incomingControl.value(), sourceBounds, previewBounds);
                        auto *connector = scene->addLine(QLineF(anchorPreview, controlPreview),
                                                         cosmeticPen(canvasTheme.controlConnector, controlConnectorWidth, Qt::DashLine, Qt::RoundCap));
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
                        registerVertexItem(controlItem);
                        controlItemsBySourceVertex->insert(currentVertex.incomingSourceVertexIndex, controlItem);
                    }
                }
                if (feature.closed && feature.lineVertices.size() >= 2) {
                    const MapGeometryFeature::TH2LineVertex &lastVertex = feature.lineVertices.last();
                    const MapGeometryFeature::TH2LineVertex &firstVertex = feature.lineVertices.first();

                    if (lastVertex.outgoingControl.has_value() && lastVertex.outgoingSourceVertexIndex >= 0) {
                        const QPointF anchorPreview = mapGeometryPointToPreview(lastVertex.anchor, sourceBounds, previewBounds);
                        const QPointF controlPreview = mapGeometryPointToPreview(lastVertex.outgoingControl.value(), sourceBounds, previewBounds);
                        auto *connector = scene->addLine(QLineF(anchorPreview, controlPreview),
                                                         cosmeticPen(canvasTheme.controlConnector, controlConnectorWidth, Qt::DashLine, Qt::RoundCap));
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
                        registerVertexItem(controlItem);
                        controlItemsBySourceVertex->insert(lastVertex.outgoingSourceVertexIndex, controlItem);
                    }

                    if (firstVertex.incomingControl.has_value() && firstVertex.incomingSourceVertexIndex >= 0) {
                        const QPointF anchorPreview = mapGeometryPointToPreview(firstVertex.anchor, sourceBounds, previewBounds);
                        const QPointF controlPreview = mapGeometryPointToPreview(firstVertex.incomingControl.value(), sourceBounds, previewBounds);
                        auto *connector = scene->addLine(QLineF(anchorPreview, controlPreview),
                                                         cosmeticPen(canvasTheme.controlConnector, controlConnectorWidth, Qt::DashLine, Qt::RoundCap));
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
                        registerVertexItem(controlItem);
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
                const auto currentInteractiveLineFeature = [feature,
                                                            previewToSource,
                                                            anchorItemsByOrder,
                                                            controlItemsBySourceVertex]() {
                    MapGeometryFeature interactiveFeature = feature;
                    for (int index = 0; index < interactiveFeature.lineVertices.size(); ++index) {
                        MapGeometryFeature::TH2LineVertex &vertex = interactiveFeature.lineVertices[index];
                        if (anchorItemsByOrder != nullptr && index >= 0 && index < anchorItemsByOrder->size()) {
                            if (MapEditableGeometryVertexItem *item = anchorItemsByOrder->at(index)) {
                                vertex.anchor = previewToSource(item->pos());
                            }
                        }
                        if (vertex.incomingSourceVertexIndex >= 0 && controlItemsBySourceVertex != nullptr) {
                            if (MapEditableGeometryVertexItem *control = controlItemsBySourceVertex->value(vertex.incomingSourceVertexIndex, nullptr)) {
                                vertex.incomingControl = previewToSource(control->pos());
                            }
                        }
                        if (vertex.outgoingSourceVertexIndex >= 0 && controlItemsBySourceVertex != nullptr) {
                            if (MapEditableGeometryVertexItem *control = controlItemsBySourceVertex->value(vertex.outgoingSourceVertexIndex, nullptr)) {
                                vertex.outgoingControl = previewToSource(control->pos());
                            }
                        }
                    }
                    return interactiveFeature;
                };
                const auto updateInteractiveLinePreview = [lineItem,
                                                           lineDecorationItem,
                                                           lineGuideSpineItem,
                                                           styledLineItems,
                                                           styledDecorationItems,
                                                           styledGuideItems,
                                                           lineLabelItem,
                                                           directionTickItem,
                                                           lineDirectionTickLength,
                                                           feature,
                                                           sourceBounds,
                                                           previewBounds,
                                                           anchorItemsByOrder,
                                                           controlItemsBySourceVertex,
                                                           controlConnectors,
                                                           currentInteractiveLineFeature]() {
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

                    const MapGeometryFeature interactiveFeature = currentInteractiveLineFeature();
                    const QPainterPath interactivePath = linePathForFeature(interactiveFeature, sourceBounds, previewBounds);
                    lineItem->setPath(interactivePath);
                    if (lineDecorationItem != nullptr) {
                        lineDecorationItem->setDecorationPath(interactivePath);
                    }
                    if (lineGuideSpineItem != nullptr) {
                        lineGuideSpineItem->setPath(interactivePath);
                    }
                    if (lineLabelItem != nullptr) {
                        lineLabelItem->setPath(interactivePath);
                    }
                    const QVector<StyledLinePath> interactiveStyledPaths =
                        styledLinePathsForFeature(interactiveFeature, sourceBounds, previewBounds);
                    if (styledLineItems != nullptr) {
                        for (const StyledPathItemBinding &binding : *styledLineItems) {
                            if (binding.pathItem == nullptr
                                || binding.styledPathIndex < 0
                                || binding.styledPathIndex >= interactiveStyledPaths.size()) {
                                continue;
                            }
                            binding.pathItem->setPath(interactiveStyledPaths.at(binding.styledPathIndex).path);
                        }
                    }
                    if (styledDecorationItems != nullptr) {
                        for (const StyledLineDecorationBinding &binding : *styledDecorationItems) {
                            if (binding.decorationItem == nullptr
                                || binding.styledPathIndex < 0
                                || binding.styledPathIndex >= interactiveStyledPaths.size()) {
                                continue;
                            }
                            binding.decorationItem->setDecorationPath(interactiveStyledPaths.at(binding.styledPathIndex).path);
                        }
                    }
                    if (styledGuideItems != nullptr) {
                        for (const StyledPathItemBinding &binding : *styledGuideItems) {
                            if (binding.pathItem == nullptr
                                || binding.styledPathIndex < 0
                                || binding.styledPathIndex >= interactiveStyledPaths.size()) {
                                continue;
                            }
                            binding.pathItem->setPath(interactiveStyledPaths.at(binding.styledPathIndex).path);
                        }
                    }
                    std::optional<QPointF> outgoingControlPreview;
                    const MapGeometryFeature::TH2LineVertex &firstVertex = interactiveFeature.lineVertices.first();
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
                                              updateInteractiveLinePreview,
                                              currentInteractiveLineFeature](MapEditableGeometryVertexItem *movedItem,
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

                    const MapGeometryFeature interactiveFeature = currentInteractiveLineFeature();
                    const QVector<MapLineSecondaryMove> moves = collectLinePreviewCoupledUpdatesForVertexDrag(interactiveFeature,
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

                break;
            }
            case MapGeometryFeature::Kind::Area: {
                if (feature.vertices.size() < 3) {
                    break;
                }
                const QString featureTooltip = geometryTooltipForFeature(feature);

                const MapEditorResolvedAreaStyle areaStyle = resolveMapEditorAreaStyle(styleCatalog,
                                                                                       feature.label,
                                                                                       feature.subtype);
                const qreal areaStrokeBase = qBound(0.8, areaStyle.strokeWidth, 24.0);
                const qreal areaStrokeWidth = qBound(0.8, areaStrokeBase * mapScale, areaStrokeBase * 1.2);
                QColor areaFillColor = areaStyle.fillColor.value_or(canvasTheme.areaFill);
                areaFillColor.setAlphaF(qBound(0.0, areaStyle.fillOpacity, 1.0));
                const QColor areaStrokeColor = areaStyle.strokeColor.value_or(canvasTheme.geometryStroke);
                const AreaRenderZValues areaZ = areaRenderZValues(feature.areaPlace);
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
                QPainterPath renderPath = path;
                if (feature.clipToScrap && feature.scrapLineNumber > 0) {
                    const QPainterPath scrapClipPath = scrapClipPaths.value(feature.scrapLineNumber);
                    if (!scrapClipPath.isEmpty()) {
                        QPainterPath clippedPath = path.intersected(scrapClipPath);
                        clippedPath.setFillRule(Qt::OddEvenFill);
                        if (!clippedPath.isEmpty() && !clippedPath.boundingRect().isEmpty()) {
                            renderPath = clippedPath;
                        }
                    }
                }

                QBrush areaBrush(areaFillColor, Qt::SolidPattern);

                auto *fillItem = new MapZoomAwarePathItem(renderPath,
                                                          styledGeometricPen(areaStrokeColor,
                                                                             areaStrokeWidth,
                                                                             areaStyle.penStyle,
                                                                             areaStyle.dashPattern,
                                                                             Qt::RoundCap,
                                                                             Qt::RoundJoin),
                                                          areaBrush);
                scene->addItem(fillItem);
                fillItem->setFlag(QGraphicsItem::ItemIsSelectable, true);
                fillItem->setZValue(areaZ.fill);
                fillItem->setToolTip(featureTooltip);
                markGeometryItem(fillItem);
                fillItem->setData(kMapSceneLineNumberRole, feature.lineNumber);
                fillItem->setData(kMapSceneSelectionSubtypeRole, kMapSceneSelectionSubtypeAreaFill);
                if (mapItemsByLine != nullptr && feature.lineNumber > 0) {
                    mapItemsByLine->insert(feature.lineNumber, fillItem);
                }

                if (areaStyle.fillPattern.has_value()) {
                    QColor fallbackPatternColor = areaStrokeColor;
                    fallbackPatternColor.setAlphaF(qBound(0.0, areaFillColor.alphaF() + 0.08, 1.0));
                    auto *patternItem = new MapZoomAwareAreaPatternItem(renderPath,
                                                                        areaStyle.fillPattern.value(),
                                                                        fallbackPatternColor,
                                                                        feature.lineNumber);
                    scene->addItem(patternItem);
                    patternItem->setZValue(areaZ.pattern);
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
                        registerVertexItem(vertexItem);
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
    struct PendingReferencedArea
    {
        MapGeometryFeature feature;
        QStringList borderIdentifiers;
    };

    QVector<MapGeometryFeature> features;
    MapGeometryFeature currentFeature;
    bool inLineBlock = false;
    bool inAreaBlock = false;
    int lineSourceVertexIndex = 0;
    QString currentLineIdentifier;
    QString currentLineSegmentSubtype;
    QStringList currentAreaBorderIdentifiers;
    QHash<QString, MapGeometryFeature> lineFeaturesByIdentifier;
    QVector<PendingReferencedArea> pendingReferencedAreas;
    QVector<int> scrapLineStack;
    auto currentScrapLineNumber = [&]() {
        return scrapLineStack.isEmpty() ? 0 : scrapLineStack.last();
    };

    auto resolveReferencedArea = [&](MapGeometryFeature *feature, const QStringList &borderIdentifiers) {
        if (feature == nullptr || borderIdentifiers.isEmpty()) {
            return false;
        }

        const MapEditorReferencedAreaResolution resolution =
            resolveMapEditorReferencedArea(borderIdentifiers, lineFeaturesByIdentifier);
        if (resolution.vertices.size() < 3) {
            return false;
        }

        feature->vertices = resolution.vertices;
        feature->lineVertices = resolution.lineVertices;
        feature->closed = true;
        feature->verticesEditable = false;
        return true;
    };

    auto resolvePendingReferencedAreas = [&]() {
        QVector<PendingReferencedArea> unresolved;
        for (PendingReferencedArea pendingArea : std::as_const(pendingReferencedAreas)) {
            if (resolveReferencedArea(&pendingArea.feature, pendingArea.borderIdentifiers)) {
                features.append(pendingArea.feature);
            } else {
                unresolved.append(pendingArea);
            }
        }
        pendingReferencedAreas = unresolved;
    };

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
                if (resolveReferencedArea(&currentFeature, currentAreaBorderIdentifiers)) {
                    features.append(currentFeature);
                } else {
                    pendingReferencedAreas.append(PendingReferencedArea{currentFeature, currentAreaBorderIdentifiers});
                }
            }
        }

        currentFeature = MapGeometryFeature{};
        inLineBlock = false;
        inAreaBlock = false;
        lineSourceVertexIndex = 0;
        currentLineIdentifier.clear();
        currentLineSegmentSubtype.clear();
        currentAreaBorderIdentifiers.clear();
    };

    for (const TherionParsedLine &parsedLine : parsedLines) {
        const QString directive = parsedLine.directive;

        if (!inLineBlock && !inAreaBlock && directive == QStringLiteral("scrap")) {
            flushCurrentFeature();
            resolvePendingReferencedAreas();
            lineFeaturesByIdentifier.clear();
            pendingReferencedAreas.clear();
            scrapLineStack.append(parsedLine.lineNumber);
            continue;
        }

        if (!inLineBlock && !inAreaBlock && directive == QStringLiteral("endscrap")) {
            flushCurrentFeature();
            resolvePendingReferencedAreas();
            if (!scrapLineStack.isEmpty()) {
                scrapLineStack.removeLast();
            }
            if (scrapLineStack.isEmpty()) {
                lineFeaturesByIdentifier.clear();
                pendingReferencedAreas.clear();
            }
            continue;
        }

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

            const QString pointType = pointTypeTokenFromLine(parsedLine);
            MapGeometryFeature feature;
            feature.kind = MapGeometryFeature::Kind::Point;
            feature.lineNumber = parsedLine.lineNumber;
            feature.scrapLineNumber = currentScrapLineNumber();
            feature.category = mapEntryCategoryForLine(parsedLine);
            feature.label = pointType.isEmpty() ? mapEntryTitleForLine(parsedLine) : geometryFeatureTypePart(pointType);
            feature.subtype = geometryFeatureSubtypeFromParsedLine(parsedLine, pointType);
            feature.subtitle = mapEntrySubtitleForLine(parsedLine);
            feature.optionValues = commandOptionValuesByName(parsedLine.tokens);
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
            feature.stationPoint = geometryFeatureTypePart(pointType) == QStringLiteral("station");
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
            feature.scrapLineNumber = currentScrapLineNumber();
            feature.category = mapEntryCategoryForLine(parsedLine);
            feature.label = mapEntryTitleForLine(parsedLine);
            feature.subtype = geometryFeatureSubtypeFromParsedLine(parsedLine, pointTypeTokenFromLine(parsedLine));
            feature.subtitle = mapEntrySubtitleForLine(parsedLine);
            feature.optionValues = commandOptionValuesByName(parsedLine.tokens);
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
            currentFeature.scrapLineNumber = currentScrapLineNumber();
            currentFeature.category = mapEntryCategoryForLine(parsedLine);
            currentFeature.label = geometryFeatureTypePart(parsedLine.tokens.value(1));
            currentFeature.subtype = geometryFeatureSubtypeFromParsedLine(parsedLine, parsedLine.tokens.value(1));
            currentLineSegmentSubtype = currentFeature.subtype.trimmed().toLower();
            currentFeature.subtitle = mapEntrySubtitleForLine(parsedLine);
            currentFeature.optionValues = commandOptionValuesByName(parsedLine.tokens);
            currentFeature.accent = mapEntryAccentForCategory(currentFeature.category);
            currentFeature.closed = commandOptionToggleValue(parsedLine.tokens, QStringLiteral("-close")).value_or(false);
            currentFeature.reversed = commandOptionToggleValue(parsedLine.tokens, QStringLiteral("-reverse")).value_or(false);
            currentLineIdentifier = commandOptionValue(parsedLine.tokens, QStringLiteral("-id"));
            appendLineDataPoints(&currentFeature,
                                 sourceCoordinatePointsFromLine(parsedLine, 1, &lineSourceVertexIndex),
                                 currentLineSegmentSubtype);
            applyLinePointOptionsFromLine(parsedLine, &currentFeature);
            inLineBlock = true;
            continue;
        }

        if (!inLineBlock && !inAreaBlock && directive == QStringLiteral("area")) {
            flushCurrentFeature();
            currentFeature.kind = MapGeometryFeature::Kind::Area;
            currentFeature.lineNumber = parsedLine.lineNumber;
            currentFeature.scrapLineNumber = currentScrapLineNumber();
            currentFeature.category = mapEntryCategoryForLine(parsedLine);
            currentFeature.label = geometryFeatureTypePart(parsedLine.tokens.value(1));
            currentFeature.subtype = geometryFeatureSubtypeFromParsedLine(parsedLine, parsedLine.tokens.value(1));
            currentFeature.subtitle = mapEntrySubtitleForLine(parsedLine);
            currentFeature.accent = mapEntryAccentForCategory(currentFeature.category);
            if (const std::optional<bool> clip = commandOptionToggleValue(parsedLine.tokens, QStringLiteral("-clip"))) {
                currentFeature.clipToScrap = clip.value();
            } else {
                currentFeature.clipToScrap = commandOptionToggleValue(parsedLine.tokens, QStringLiteral("clip")).value_or(true);
            }
            QString placeValue = commandOptionValue(parsedLine.tokens, QStringLiteral("-place"));
            if (placeValue.isEmpty()) {
                placeValue = commandOptionValue(parsedLine.tokens, QStringLiteral("place"));
            }
            currentFeature.areaPlace = areaPlaceFromToken(placeValue);
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

            if (directive == QStringLiteral("subtype") && parsedLine.tokens.size() >= 2) {
                currentLineSegmentSubtype = parsedLine.tokens.at(1).trimmed().toLower();
                if (!currentFeature.lineVertices.isEmpty()) {
                    const QString trimmedRow = parsedLine.rawText.trimmed();
                    if (!trimmedRow.isEmpty()) {
                        currentFeature.lineVertices.last().standaloneOptionRows.append(trimmedRow);
                    }
                }
                continue;
            }

            const QVector<SourceCoordinatePoint> sourcePoints =
                sourceCoordinatePointsFromLine(parsedLine, 0, &lineSourceVertexIndex);
            appendLineDataPoints(&currentFeature, sourcePoints, currentLineSegmentSubtype);
            const ParsedLinePointOptionFlags parsedOptionFlags = applyLinePointOptionsFromLine(parsedLine, &currentFeature);
            if (sourcePoints.isEmpty()
                && !currentFeature.lineVertices.isEmpty()) {
                const QString trimmedRow = parsedLine.rawText.trimmed();
                const bool consumedAsStructuredStandaloneRow =
                    (parsedOptionFlags.parsedOrientation && isOrientationStandaloneDirective(parsedLine.directive))
                    || (parsedOptionFlags.parsedLeftSize && isLeftSizeStandaloneDirective(parsedLine.directive));
                if (!trimmedRow.isEmpty() && !consumedAsStructuredStandaloneRow) {
                    currentFeature.lineVertices.last().standaloneOptionRows.append(trimmedRow);
                }
            }
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

    flushCurrentFeature();
    resolvePendingReferencedAreas();

    return features;
}

QVector<MapGeometryFeature> collectGeometryFeatures(const Th2GeometryProjection &projection,
                                                    const QVector<TherionSourceLogicalCommand> &commands)
{
    QVector<TherionParsedLine> parsedLines;
    parsedLines.reserve(commands.size());
    for (const TherionSourceLogicalCommand &command : commands) {
        parsedLines.append(command.parsed);
    }

    QHash<int, QVector<MapGeometryFeature>> featuresByLine;
    for (const MapGeometryFeature &feature : collectGeometryFeatures(parsedLines)) {
        featuresByLine[feature.lineNumber].append(feature);
    }

    struct ProjectedFeatureRef
    {
        int lineNumber = 0;
        MapGeometryFeature::Kind kind = MapGeometryFeature::Kind::Point;
    };

    QVector<ProjectedFeatureRef> projectedRefs;
    projectedRefs.reserve(projection.points().size() + projection.lines().size() + projection.areas().size());
    for (const Th2PointObject &point : projection.points()) {
        projectedRefs.append({point.command.sourceRange.startLineNumber, MapGeometryFeature::Kind::Point});
    }
    for (const Th2LineObject &line : projection.lines()) {
        projectedRefs.append({line.command.sourceRange.startLineNumber, MapGeometryFeature::Kind::Line});
    }
    for (const Th2AreaObject &area : projection.areas()) {
        projectedRefs.append({area.command.sourceRange.startLineNumber, MapGeometryFeature::Kind::Area});
    }
    std::sort(projectedRefs.begin(), projectedRefs.end(), [](const ProjectedFeatureRef &a, const ProjectedFeatureRef &b) {
        return a.lineNumber < b.lineNumber;
    });

    QVector<MapGeometryFeature> features;
    features.reserve(projectedRefs.size());
    for (const ProjectedFeatureRef &ref : std::as_const(projectedRefs)) {
        if (ref.lineNumber <= 0) {
            continue;
        }
        const QVector<MapGeometryFeature> candidates = featuresByLine.value(ref.lineNumber);
        for (const MapGeometryFeature &candidate : candidates) {
            if (candidate.kind == ref.kind) {
                features.append(candidate);
                break;
            }
        }
    }
    return features;
}

} // namespace TherionStudio
