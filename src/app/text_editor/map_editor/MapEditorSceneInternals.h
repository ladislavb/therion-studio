#pragma once

#include <QColor>
#include <QCoreApplication>
#include <QCursor>
#include <QGraphicsEllipseItem>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QPainterPathStroker>
#include <QPointF>
#include <QRectF>
#include <QObject>
#include <QStringList>
#include <QStyle>
#include <QStyleOptionGraphicsItem>
#include <QTimer>
#include <QTransform>
#include <QVariant>
#include <QVector>
#include <functional>
#include <cmath>

namespace TherionStudio {

inline bool mapSourcePointsDiffer(const QPointF &a, const QPointF &b)
{
    return (a - b).manhattanLength() > 0.01;
}

inline qreal mapZoomOutMarkerScale(const QGraphicsItem *item, const QPainter *painter, const QWidget *widget)
{
    qreal lod = 0.0;

    if (widget != nullptr) {
        const QObject *parent = widget->parent();
        const QGraphicsView *view = qobject_cast<const QGraphicsView *>(parent);
        if (view != nullptr) {
            const QTransform viewTransform = view->transform();
            const qreal viewScale = std::hypot(viewTransform.m11(), viewTransform.m21());
            if (viewScale > 0.0) {
                lod = viewScale;
            }
        }
    }

    if (lod <= 0.0 && item != nullptr && item->scene() != nullptr) {
        const QList<QGraphicsView *> views = item->scene()->views();
        for (const QGraphicsView *view : views) {
            if (view == nullptr) {
                continue;
            }
            const QTransform viewTransform = view->transform();
            const qreal viewScale = std::hypot(viewTransform.m11(), viewTransform.m21());
            if (viewScale > lod) {
                lod = viewScale;
            }
        }
    }

    if (lod <= 0.0 && painter != nullptr) {
        lod = QStyleOptionGraphicsItem::levelOfDetailFromTransform(painter->worldTransform());
    }
    if (lod <= 0.0) {
        lod = 1.0;
    }

    if (lod >= 1.0) {
        return 1.0;
    }

    return qBound<qreal>(0.34, std::pow(qMax<qreal>(0.01, lod), 0.70), 1.0);
}

struct CoordinateTransform
{
    bool valid = false;
    QTransform sourceToMap;
    QTransform mapToSource;
};

QRectF sceneCoordsPreviewBounds(const QRectF &sourceBounds, const QRectF &targetBounds);
QPointF sceneCoordsSourceToPreview(const QPointF &source, const QRectF &sourceBounds, const QRectF &previewBounds);
QPointF sceneCoordsPreviewToSource(const QPointF &preview, const QRectF &sourceBounds, const QRectF &previewBounds);
qreal sceneCoordsScaleFactor(const QRectF &sourceBounds, const QRectF &previewBounds);
CoordinateTransform coordinateTransformFromScrapScale(const QStringList &tokens);
QVector<QPointF> pointsFromTokens(const QStringList &tokens);

class MapEditablePointItem final : public QGraphicsEllipseItem
{
public:
    MapEditablePointItem(int lineNumber,
                         const QPointF &sourcePoint,
                         const QRectF &sourceBounds,
                         const QRectF &previewBounds,
                         bool isStation = false)
        : QGraphicsEllipseItem(QRectF(-5.0, -5.0, 10.0, 10.0))
        , lineNumber_(lineNumber)
        , sourceBounds_(sourceBounds)
        , previewBounds_(previewBounds)
        , fittedBounds_(previewBounds)
        , isStation_(isStation)
    {
        setFlag(QGraphicsItem::ItemIsMovable, true);
        setFlag(QGraphicsItem::ItemIsSelectable, true);
        setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
        setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
        setAcceptHoverEvents(true);
        setCursor(Qt::OpenHandCursor);
        setToolTip(QObject::tr("Point handle (line %1)").arg(lineNumber_));
        setPos(sceneCoordsSourceToPreview(sourcePoint, sourceBounds, previewBounds));
    }

    void setMoveCommittedCallback(std::function<void(int, const QPointF &, const QPointF &)> callback)
    {
        moveCommittedCallback_ = std::move(callback);
    }

    void setDisplayToSourceMapper(std::function<QPointF(const QPointF &)> mapper)
    {
        displayToSourceMapper_ = std::move(mapper);
    }

    QPointF sourcePoint() const
    {
        return mapDisplayToSource(previewToSource(pos()));
    }

protected:
    QRectF boundingRect() const override
    {
        // Paint includes scaled marker plus halo; keep update region larger to avoid drag trails.
        return QGraphicsEllipseItem::boundingRect().adjusted(-6.0, -6.0, 6.0, 6.0);
    }

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override
    {
        const bool selected = option != nullptr && (option->state & QStyle::State_Selected);
        const bool emphasize = selected || hoverActive_ || dragActive_;
        const qreal interactionScale = dragActive_ ? 1.5 : (selected ? 1.4 : (hoverActive_ ? 1.25 : 1.0));
        const qreal zoomOutScale = mapZoomOutMarkerScale(this, painter, widget);
        const qreal scale = interactionScale * zoomOutScale;
        const QRectF baseRect = rect();
        const QPointF center = baseRect.center();
        QRectF drawRect = baseRect;
        drawRect.setWidth(baseRect.width() * scale);
        drawRect.setHeight(baseRect.height() * scale);
        drawRect.moveCenter(center);

        const QColor baseFill = brush().color().isValid() ? brush().color() : QColor(24, 24, 24, 170);
        QColor fill = baseFill;
        fill.setAlpha(emphasize ? 235 : 180);
        QColor outline = selected ? QColor(QStringLiteral("#3ba4ff")) : QColor(22, 22, 22, 210);

        painter->setRenderHint(QPainter::Antialiasing, true);
        if (emphasize) {
            QColor halo = selected
                ? QColor(72, 166, 255, 110)
                : (fill.lightnessF() > 0.6 ? QColor(24, 38, 56, 70) : QColor(255, 255, 255, 70));
            painter->setPen(Qt::NoPen);
            painter->setBrush(halo);
            const qreal haloRadius = 2.2 * zoomOutScale;
            painter->drawEllipse(drawRect.adjusted(-haloRadius, -haloRadius, haloRadius, haloRadius));
        }

        const qreal outlineWidth = (selected ? 1.8 : 1.1) * zoomOutScale;
        painter->setPen(QPen(outline, qMax<qreal>(0.55, outlineWidth)));
        painter->setBrush(fill);
        if (isStation_) {
            QPolygonF triangle;
            triangle << QPointF(drawRect.center().x(), drawRect.top())
                     << QPointF(drawRect.right(), drawRect.bottom())
                     << QPointF(drawRect.left(), drawRect.bottom());
            painter->drawPolygon(triangle);
        } else {
            painter->drawEllipse(drawRect);
        }
    }

    void mousePressEvent(QGraphicsSceneMouseEvent *event) override
    {
        pressSourcePoint_ = mapDisplayToSource(previewToSource(pos()));
        dragActive_ = true;
        setCursor(Qt::ClosedHandCursor);
        QGraphicsEllipseItem::mousePressEvent(event);
    }

    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override
    {
        dragActive_ = false;
        setCursor(Qt::OpenHandCursor);
        QGraphicsEllipseItem::mouseReleaseEvent(event);

        const QPointF releasedSourcePoint = mapDisplayToSource(previewToSource(pos()));
        if (moveCommittedCallback_ != nullptr && mapSourcePointsDiffer(pressSourcePoint_, releasedSourcePoint)) {
            moveCommittedCallback_(lineNumber_, pressSourcePoint_, releasedSourcePoint);
            // The callback may trigger a scene rebuild that deletes this item.
            return;
        }
        update();
    }

    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override
    {
        hoverActive_ = true;
        QGraphicsEllipseItem::hoverEnterEvent(event);
        update();
    }

    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override
    {
        hoverActive_ = false;
        QGraphicsEllipseItem::hoverLeaveEvent(event);
        update();
    }

    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override
    {
        if (change == QGraphicsItem::ItemPositionChange) {
            QPointF candidate = value.toPointF();
            candidate.setX(qBound(fittedBounds_.left(), candidate.x(), fittedBounds_.right()));
            candidate.setY(qBound(fittedBounds_.top(), candidate.y(), fittedBounds_.bottom()));
            return candidate;
        }

        return QGraphicsEllipseItem::itemChange(change, value);
    }

private:
    QPointF mapDisplayToSource(const QPointF &displayPoint) const
    {
        if (displayToSourceMapper_ != nullptr) {
            return displayToSourceMapper_(displayPoint);
        }

        return displayPoint;
    }

    QPointF previewToSource(const QPointF &previewPoint) const
    {
        return sceneCoordsPreviewToSource(previewPoint, sourceBounds_, previewBounds_);
    }

    int lineNumber_ = 0;
    QRectF sourceBounds_;
    QRectF previewBounds_;
    QRectF fittedBounds_;
    QPointF pressSourcePoint_;
    bool hoverActive_ = false;
    bool dragActive_ = false;
    bool isStation_ = false;
    std::function<QPointF(const QPointF &)> displayToSourceMapper_;
    std::function<void(int, const QPointF &, const QPointF &)> moveCommittedCallback_;
};

class MapEditableGeometryVertexItem final : public QGraphicsEllipseItem
{
public:
    MapEditableGeometryVertexItem(int lineNumber,
                                  const QString &geometryKind,
                                  int vertexIndex,
                                  const QPointF &sourcePoint,
                                  const QRectF &sourceBounds,
                                  const QRectF &previewBounds)
        : QGraphicsEllipseItem(QRectF(-4.0, -4.0, 8.0, 8.0))
        , lineNumber_(lineNumber)
        , geometryKind_(geometryKind)
        , vertexIndex_(vertexIndex)
        , sourceBounds_(sourceBounds)
        , previewBounds_(previewBounds)
        , fittedBounds_(previewBounds)
    {
        setFlag(QGraphicsItem::ItemIsMovable, true);
        setFlag(QGraphicsItem::ItemIsSelectable, true);
        setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
        setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
        setAcceptHoverEvents(true);
        setCursor(Qt::OpenHandCursor);
        setToolTip(QObject::tr("%1 vertex %2 (line %3)")
                       .arg(geometryKind_.isEmpty() ? QObject::tr("Geometry") : geometryKind_)
                       .arg(vertexIndex_ + 1)
                       .arg(lineNumber_));
        setPos(sceneCoordsSourceToPreview(sourcePoint, sourceBounds, previewBounds));
    }

    int lineNumber() const
    {
        return lineNumber_;
    }

    QString geometryKind() const
    {
        return geometryKind_;
    }

    int vertexIndex() const
    {
        return vertexIndex_;
    }

    void setMoveCommittedCallback(std::function<void(int, const QString &, int, const QPointF &, const QPointF &)> callback)
    {
        moveCommittedCallback_ = std::move(callback);
    }

    void setDisplayToSourceMapper(std::function<QPointF(const QPointF &)> mapper)
    {
        displayToSourceMapper_ = std::move(mapper);
    }

    void setMovePreviewCallback(std::function<void(MapEditableGeometryVertexItem *,
                                                   const QPointF &,
                                                   const QPointF &,
                                                   bool)> callback)
    {
        movePreviewCallback_ = std::move(callback);
    }

protected:
    QRectF boundingRect() const override
    {
        // Paint includes scaled marker plus halo; keep update region larger to avoid drag trails.
        return QGraphicsEllipseItem::boundingRect().adjusted(-6.0, -6.0, 6.0, 6.0);
    }

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override
    {
        const bool selected = option != nullptr && (option->state & QStyle::State_Selected);
        const bool emphasize = selected || hoverActive_ || dragActive_;
        const qreal interactionScale = dragActive_ ? 1.45 : (selected ? 1.35 : (hoverActive_ ? 1.2 : 1.0));
        const qreal zoomOutScale = mapZoomOutMarkerScale(this, painter, widget);
        const qreal scale = interactionScale * zoomOutScale;
        const QRectF baseRect = rect();
        const QPointF center = baseRect.center();
        QRectF drawRect = baseRect;
        drawRect.setWidth(baseRect.width() * scale);
        drawRect.setHeight(baseRect.height() * scale);
        drawRect.moveCenter(center);

        QColor fill = brush().color().isValid() ? brush().color() : QColor(40, 40, 40, 160);
        fill.setAlpha(emphasize ? 240 : 175);
        QColor outline = selected ? QColor(QStringLiteral("#3ba4ff")) : pen().color();
        if (!outline.isValid()) {
            outline = QColor(24, 24, 24, 210);
        }

        painter->setRenderHint(QPainter::Antialiasing, true);
        if (emphasize) {
            QColor halo = selected
                ? QColor(72, 166, 255, 110)
                : (fill.lightnessF() > 0.6 ? QColor(24, 38, 56, 70) : QColor(255, 255, 255, 70));
            painter->setPen(Qt::NoPen);
            painter->setBrush(halo);
            const qreal haloRadius = 2.0 * zoomOutScale;
            painter->drawEllipse(drawRect.adjusted(-haloRadius, -haloRadius, haloRadius, haloRadius));
        }

        const qreal outlineWidth = (selected ? 1.7 : 1.0) * zoomOutScale;
        painter->setPen(QPen(outline, qMax<qreal>(0.5, outlineWidth)));
        painter->setBrush(fill);
        painter->drawEllipse(drawRect);
    }

    void mousePressEvent(QGraphicsSceneMouseEvent *event) override
    {
        pressSourcePoint_ = sourcePointForPreviewPos(pos());
        lastPreviewSourcePoint_ = pressSourcePoint_;
        dragActive_ = true;
        setCursor(Qt::ClosedHandCursor);
        QGraphicsEllipseItem::mousePressEvent(event);
    }

    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override
    {
        dragActive_ = false;
        setCursor(Qt::OpenHandCursor);
        QGraphicsEllipseItem::mouseReleaseEvent(event);

        const QPointF releasedSourcePoint = sourcePointForPreviewPos(pos());
        if (moveCommittedCallback_ != nullptr && mapSourcePointsDiffer(pressSourcePoint_, releasedSourcePoint)) {
            moveCommittedCallback_(lineNumber_, geometryKind_, vertexIndex_, pressSourcePoint_, releasedSourcePoint);
            // The callback may trigger a scene rebuild that deletes this item.
            return;
        }
        update();
    }

    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override
    {
        hoverActive_ = true;
        QGraphicsEllipseItem::hoverEnterEvent(event);
        update();
    }

    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override
    {
        hoverActive_ = false;
        QGraphicsEllipseItem::hoverLeaveEvent(event);
        update();
    }

    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override
    {
        if (change == QGraphicsItem::ItemPositionChange) {
            QPointF candidate = value.toPointF();
            candidate.setX(qBound(fittedBounds_.left(), candidate.x(), fittedBounds_.right()));
            candidate.setY(qBound(fittedBounds_.top(), candidate.y(), fittedBounds_.bottom()));
            return candidate;
        }
        if (change == QGraphicsItem::ItemPositionHasChanged && movePreviewCallback_ != nullptr) {
            const QPointF newSourcePoint = sourcePointForPreviewPos(pos());
            movePreviewCallback_(this, lastPreviewSourcePoint_, newSourcePoint, dragActive_);
            lastPreviewSourcePoint_ = newSourcePoint;
        }
        return QGraphicsEllipseItem::itemChange(change, value);
    }

private:
    QPointF sourcePointForPreviewPos(const QPointF &previewPoint) const
    {
        return mapDisplayToSource(previewToSource(previewPoint));
    }

    QPointF mapDisplayToSource(const QPointF &displayPoint) const
    {
        if (displayToSourceMapper_ != nullptr) {
            return displayToSourceMapper_(displayPoint);
        }

        return displayPoint;
    }

    QPointF previewToSource(const QPointF &previewPoint) const
    {
        return sceneCoordsPreviewToSource(previewPoint, sourceBounds_, previewBounds_);
    }

    int lineNumber_ = 0;
    QString geometryKind_;
    int vertexIndex_ = -1;
    QRectF sourceBounds_;
    QRectF previewBounds_;
    QRectF fittedBounds_;
    QPointF pressSourcePoint_;
    QPointF lastPreviewSourcePoint_;
    bool hoverActive_ = false;
    bool dragActive_ = false;
    std::function<QPointF(const QPointF &)> displayToSourceMapper_;
    std::function<void(MapEditableGeometryVertexItem *, const QPointF &, const QPointF &, bool)> movePreviewCallback_;
    std::function<void(int, const QString &, int, const QPointF &, const QPointF &)> moveCommittedCallback_;
};

class MapLinePointSizeHandleItem final : public QGraphicsItem
{
public:
    MapLinePointSizeHandleItem(int lineNumber,
                               int sourceVertexIndex,
                               const QPointF &anchorPreview,
                               qreal orientationDegrees,
                               qreal leftSize,
                               qreal previewScale)
        : lineNumber_(lineNumber)
        , sourceVertexIndex_(sourceVertexIndex)
        , anchorPreview_(anchorPreview)
        , orientationDegrees_(normalizeDegrees(orientationDegrees))
        , leftSize_(qMax<qreal>(0.1, leftSize))
        , previewScale_(qMax<qreal>(1e-6, previewScale))
    {
        setFlag(QGraphicsItem::ItemIsSelectable, true);
        setAcceptHoverEvents(true);
        setAcceptedMouseButtons(Qt::LeftButton);
        setCursor(Qt::OpenHandCursor);
        setToolTip(QObject::tr("Line point orientation / l-size handle (line %1)").arg(lineNumber_));
        handlePreview_ = handlePointForValues(orientationDegrees_, leftSize_);
    }

    void setChangeCommittedCallback(std::function<void(int, int, qreal, qreal)> callback)
    {
        changeCommittedCallback_ = std::move(callback);
    }

    QRectF boundingRect() const override
    {
        return QRectF(anchorPreview_, handlePreview_).normalized().adjusted(-10.0, -10.0, 10.0, 10.0);
    }

    QPainterPath shape() const override
    {
        QPainterPath path;
        path.moveTo(anchorPreview_);
        path.lineTo(handlePreview_);
        path.addEllipse(handlePreview_, 6.0, 6.0);

        QPainterPathStroker stroker;
        stroker.setWidth(12.0);
        stroker.setCapStyle(Qt::RoundCap);
        stroker.setJoinStyle(Qt::RoundJoin);
        return stroker.createStroke(path).united(path);
    }

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override
    {
        Q_UNUSED(option);
        Q_UNUSED(widget);
        if (painter == nullptr) {
            return;
        }

        QColor color = (hoverActive_ || dragActive_) ? QColor(QStringLiteral("#ffda00")) : QColor(QStringLiteral("#ff0000"));
        color.setAlpha(dragActive_ ? 230 : (hoverActive_ ? 210 : 175));
        painter->setRenderHint(QPainter::Antialiasing, true);
        const QPointF vector = handlePreview_ - anchorPreview_;
        const qreal length = std::hypot(vector.x(), vector.y());
        if (length > 1e-6) {
            const QPointF direction(vector.x() / length, vector.y() / length);
            const QPointF normal(-direction.y(), direction.x());
            const QPointF arrowTip = handlePreview_;
            const qreal arrowLength = qMin<qreal>(7.0, length);
            const QPointF arrowBase = arrowTip - (direction * arrowLength);
            QPen handlePen(color, dragActive_ ? 3.0 : 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
            handlePen.setCosmetic(true);
            painter->setPen(handlePen);
            painter->drawLine(anchorPreview_, arrowBase);

            QPainterPath arrow;
            arrow.moveTo(arrowTip);
            arrow.lineTo(arrowBase + (normal * 3.5));
            arrow.lineTo(arrowBase - (normal * 3.5));
            arrow.closeSubpath();
            painter->setPen(Qt::NoPen);
            painter->setBrush(color);
            painter->drawPath(arrow);
        }
    }

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override
    {
        if (event == nullptr || event->button() != Qt::LeftButton) {
            QGraphicsItem::mousePressEvent(event);
            return;
        }

        pressOrientationDegrees_ = orientationDegrees_;
        pressLeftSize_ = leftSize_;
        dragActive_ = true;
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        updateHandleFromScenePoint(event->scenePos());
    }

    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override
    {
        if (event == nullptr || !dragActive_) {
            QGraphicsItem::mouseMoveEvent(event);
            return;
        }

        updateHandleFromScenePoint(event->scenePos());
        event->accept();
    }

    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override
    {
        if (event == nullptr || event->button() != Qt::LeftButton) {
            QGraphicsItem::mouseReleaseEvent(event);
            return;
        }

        if (dragActive_) {
            updateHandleFromScenePoint(event->scenePos());
        }
        dragActive_ = false;
        setCursor(Qt::OpenHandCursor);
        event->accept();

        const bool changed = std::fabs(orientationDegrees_ - pressOrientationDegrees_) > 0.05
            || std::fabs(leftSize_ - pressLeftSize_) > 0.05;
        if (changed && changeCommittedCallback_ != nullptr) {
            const std::function<void(int, int, qreal, qreal)> callback = changeCommittedCallback_;
            const int lineNumber = lineNumber_;
            const int sourceVertexIndex = sourceVertexIndex_;
            const qreal orientationDegrees = orientationDegrees_;
            const qreal leftSize = leftSize_;
            if (QCoreApplication::instance() != nullptr) {
                QTimer::singleShot(0,
                                   QCoreApplication::instance(),
                                   [callback, lineNumber, sourceVertexIndex, orientationDegrees, leftSize]() {
                                       callback(lineNumber, sourceVertexIndex, orientationDegrees, leftSize);
                                   });
            } else {
                callback(lineNumber, sourceVertexIndex, orientationDegrees, leftSize);
            }
            return;
        }
        update();
    }

    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override
    {
        hoverActive_ = true;
        QGraphicsItem::hoverEnterEvent(event);
        update();
    }

    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override
    {
        hoverActive_ = false;
        QGraphicsItem::hoverLeaveEvent(event);
        update();
    }

private:
    static qreal normalizeDegrees(qreal value)
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

    QPointF handlePointForValues(qreal orientationDegrees, qreal leftSize) const
    {
        constexpr qreal pi = 3.14159265358979323846;
        const qreal radians = normalizeDegrees(orientationDegrees) * pi / 180.0;
        const qreal displayLength = qBound<qreal>(12.0, qMax<qreal>(0.1, leftSize) * previewScale_, 28.0);
        return anchorPreview_ + QPointF(std::sin(radians) * displayLength,
                                        -std::cos(radians) * displayLength);
    }

    void updateHandleFromScenePoint(const QPointF &scenePoint)
    {
        const QPointF vector = scenePoint - anchorPreview_;
        const qreal displayLength = std::hypot(vector.x(), vector.y());
        if (displayLength <= 1e-6) {
            return;
        }

        prepareGeometryChange();
        handlePreview_ = scenePoint;
        constexpr qreal pi = 3.14159265358979323846;
        orientationDegrees_ = normalizeDegrees(std::atan2(vector.x(), -vector.y()) * 180.0 / pi);
        leftSize_ = qMax<qreal>(0.1, displayLength / previewScale_);
        update();
    }

    int lineNumber_ = 0;
    int sourceVertexIndex_ = -1;
    QPointF anchorPreview_;
    QPointF handlePreview_;
    qreal orientationDegrees_ = 0.0;
    qreal leftSize_ = 40.0;
    qreal previewScale_ = 1.0;
    qreal pressOrientationDegrees_ = 0.0;
    qreal pressLeftSize_ = 40.0;
    bool hoverActive_ = false;
    bool dragActive_ = false;
    std::function<void(int, int, qreal, qreal)> changeCommittedCallback_;
};

class MapPointOrientationHandleItem final : public QGraphicsItem
{
public:
    MapPointOrientationHandleItem(int lineNumber,
                                  const QPointF &anchorPreview,
                                  qreal orientationDegrees,
                                  qreal displayLength = 28.0)
        : lineNumber_(lineNumber)
        , anchorPreview_(anchorPreview)
        , orientationDegrees_(normalizeDegrees(orientationDegrees))
        , displayLength_(qMax<qreal>(12.0, displayLength))
    {
        setFlag(QGraphicsItem::ItemIsSelectable, true);
        setAcceptHoverEvents(true);
        setAcceptedMouseButtons(Qt::LeftButton);
        setCursor(Qt::OpenHandCursor);
        setToolTip(QObject::tr("Point orientation handle (line %1)").arg(lineNumber_));
        handlePreview_ = handlePointForOrientation(orientationDegrees_);
    }

    void setChangeCommittedCallback(std::function<void(int, qreal)> callback)
    {
        changeCommittedCallback_ = std::move(callback);
    }

    QRectF boundingRect() const override
    {
        return QRectF(anchorPreview_, handlePreview_).normalized().adjusted(-10.0, -10.0, 10.0, 10.0);
    }

    QPainterPath shape() const override
    {
        QPainterPath path;
        path.moveTo(anchorPreview_);
        path.lineTo(handlePreview_);
        path.addEllipse(handlePreview_, 6.0, 6.0);

        QPainterPathStroker stroker;
        stroker.setWidth(12.0);
        stroker.setCapStyle(Qt::RoundCap);
        stroker.setJoinStyle(Qt::RoundJoin);
        return stroker.createStroke(path).united(path);
    }

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override
    {
        Q_UNUSED(option);
        Q_UNUSED(widget);
        if (painter == nullptr) {
            return;
        }

        QColor color = (hoverActive_ || dragActive_) ? QColor(QStringLiteral("#ffda00")) : QColor(QStringLiteral("#ff0000"));
        color.setAlpha(dragActive_ ? 220 : (hoverActive_ ? 200 : 165));
        painter->setRenderHint(QPainter::Antialiasing, true);

        const QPointF vector = handlePreview_ - anchorPreview_;
        const qreal length = std::hypot(vector.x(), vector.y());
        if (length <= 1e-6) {
            return;
        }

        const QPointF direction(vector.x() / length, vector.y() / length);
        const QPointF normal(-direction.y(), direction.x());
        const QPointF arrowTip = handlePreview_;
        const qreal arrowLength = qMin<qreal>(7.0, length);
        const QPointF arrowBase = arrowTip - (direction * arrowLength);
        QPen handlePen(color, dragActive_ ? 3.0 : 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        handlePen.setCosmetic(true);
        painter->setPen(handlePen);
        painter->drawLine(anchorPreview_, arrowBase);

        QPainterPath arrow;
        arrow.moveTo(arrowTip);
        arrow.lineTo(arrowBase + (normal * 3.5));
        arrow.lineTo(arrowBase - (normal * 3.5));
        arrow.closeSubpath();
        painter->setPen(Qt::NoPen);
        painter->setBrush(color);
        painter->drawPath(arrow);
    }

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override
    {
        if (event == nullptr || event->button() != Qt::LeftButton) {
            QGraphicsItem::mousePressEvent(event);
            return;
        }

        pressOrientationDegrees_ = orientationDegrees_;
        dragActive_ = true;
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        updateHandleFromScenePoint(event->scenePos());
    }

    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override
    {
        if (event == nullptr || !dragActive_) {
            QGraphicsItem::mouseMoveEvent(event);
            return;
        }

        updateHandleFromScenePoint(event->scenePos());
        event->accept();
    }

    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override
    {
        if (event == nullptr || event->button() != Qt::LeftButton) {
            QGraphicsItem::mouseReleaseEvent(event);
            return;
        }

        if (dragActive_) {
            updateHandleFromScenePoint(event->scenePos());
        }
        dragActive_ = false;
        setCursor(Qt::OpenHandCursor);
        event->accept();

        const bool changed = std::fabs(orientationDegrees_ - pressOrientationDegrees_) > 0.05;
        if (changed && changeCommittedCallback_ != nullptr) {
            const std::function<void(int, qreal)> callback = changeCommittedCallback_;
            const int lineNumber = lineNumber_;
            const qreal orientationDegrees = orientationDegrees_;
            if (QCoreApplication::instance() != nullptr) {
                QTimer::singleShot(0,
                                   QCoreApplication::instance(),
                                   [callback, lineNumber, orientationDegrees]() {
                                       callback(lineNumber, orientationDegrees);
                                   });
            } else {
                callback(lineNumber, orientationDegrees);
            }
            return;
        }
        update();
    }

    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override
    {
        hoverActive_ = true;
        QGraphicsItem::hoverEnterEvent(event);
        update();
    }

    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override
    {
        hoverActive_ = false;
        QGraphicsItem::hoverLeaveEvent(event);
        update();
    }

private:
    static qreal normalizeDegrees(qreal value)
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

    QPointF handlePointForOrientation(qreal orientationDegrees) const
    {
        constexpr qreal pi = 3.14159265358979323846;
        const qreal radians = normalizeDegrees(orientationDegrees) * pi / 180.0;
        return anchorPreview_ + QPointF(std::sin(radians) * displayLength_,
                                        -std::cos(radians) * displayLength_);
    }

    void updateHandleFromScenePoint(const QPointF &scenePoint)
    {
        const QPointF vector = scenePoint - anchorPreview_;
        if (std::hypot(vector.x(), vector.y()) <= 1e-6) {
            return;
        }

        constexpr qreal pi = 3.14159265358979323846;
        orientationDegrees_ = normalizeDegrees(std::atan2(vector.x(), -vector.y()) * 180.0 / pi);
        prepareGeometryChange();
        handlePreview_ = handlePointForOrientation(orientationDegrees_);
        update();
    }

    int lineNumber_ = 0;
    QPointF anchorPreview_;
    QPointF handlePreview_;
    qreal orientationDegrees_ = 0.0;
    qreal displayLength_ = 28.0;
    qreal pressOrientationDegrees_ = 0.0;
    bool hoverActive_ = false;
    bool dragActive_ = false;
    std::function<void(int, qreal)> changeCommittedCallback_;
};

} // namespace TherionStudio
