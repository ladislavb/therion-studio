#include "MapEditorSceneSupport.h"
#include "MapEditorSceneInternals.h"

#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QFont>
#include <QLineF>
#include <QStyle>
#include <QStyleOptionGraphicsItem>
#include <QUndoCommand>

#include <algorithm>
#include <cmath>

#include "../../../core/TherionDocumentParser.h"
#include "../../../core/TherionTokenRules.h"

namespace TherionStudio {

namespace {

bool parseScaleNumber(const QString &token, qreal *value)
{
    if (value == nullptr) {
        return false;
    }

    QString normalized = token.trimmed();
    while (!normalized.isEmpty()) {
        const QChar character = normalized.front();
        if (character.isDigit() || character == QLatin1Char('+') || character == QLatin1Char('-') || character == QLatin1Char('.')) {
            break;
        }
        normalized.remove(0, 1);
    }

    while (!normalized.isEmpty()) {
        const QChar character = normalized.back();
        if (character.isDigit() || character == QLatin1Char('.')) {
            break;
        }
        normalized.chop(1);
    }

    if (normalized.isEmpty()) {
        return false;
    }

    bool ok = false;
    const qreal parsedValue = normalized.toDouble(&ok);
    if (!ok) {
        return false;
    }

    *value = parsedValue;
    return true;
}

QString normalizedScaleUnitToken(const QString &token)
{
    QString normalized;
    normalized.reserve(token.size());
    for (const QChar character : token.trimmed()) {
        if (character.isLetter()) {
            normalized.append(character.toLower());
        }
    }
    return normalized;
}

std::optional<qreal> metersPerScaleUnit(const QString &unitToken)
{
    if (unitToken.isEmpty()
        || unitToken == QStringLiteral("m")
        || unitToken == QStringLiteral("meter")
        || unitToken == QStringLiteral("meters")
        || unitToken == QStringLiteral("metre")
        || unitToken == QStringLiteral("metres")) {
        return 1.0;
    }
    if (unitToken == QStringLiteral("cm")
        || unitToken == QStringLiteral("centimeter")
        || unitToken == QStringLiteral("centimeters")
        || unitToken == QStringLiteral("centimetre")
        || unitToken == QStringLiteral("centimetres")) {
        return 0.01;
    }
    if (unitToken == QStringLiteral("mm")
        || unitToken == QStringLiteral("millimeter")
        || unitToken == QStringLiteral("millimeters")
        || unitToken == QStringLiteral("millimetre")
        || unitToken == QStringLiteral("millimetres")) {
        return 0.001;
    }
    if (unitToken == QStringLiteral("ft")
        || unitToken == QStringLiteral("foot")
        || unitToken == QStringLiteral("feet")) {
        return 0.3048;
    }
    if (unitToken == QStringLiteral("in")
        || unitToken == QStringLiteral("inch")
        || unitToken == QStringLiteral("inches")) {
        return 0.0254;
    }
    return std::nullopt;
}

QStringList scrapScaleValueTokens(const QStringList &tokens)
{
    const int scaleOptionIndex = tokens.indexOf(QStringLiteral("-scale"));
    if (scaleOptionIndex < 0 || scaleOptionIndex + 1 >= tokens.size()) {
        return {};
    }

    QStringList values;
    const QString firstValue = tokens.at(scaleOptionIndex + 1);
    const bool bracketed = firstValue.contains(QLatin1Char('['));
    for (int index = scaleOptionIndex + 1; index < tokens.size(); ++index) {
        const QString token = tokens.at(index);
        if (!bracketed && token.startsWith(QLatin1Char('-'))) {
            break;
        }

        values.append(token);
        if (!bracketed || token.contains(QLatin1Char(']'))) {
            break;
        }
    }

    return values;
}

struct ScrapScaleDefinition
{
    bool valid = false;
    QVector<qreal> numbers;
    QString unitToken;
    qreal metersPerSourceUnit = 1.0;
};

std::optional<ScrapScaleDefinition> parseScrapScaleDefinition(const QStringList &tokens)
{
    const QStringList scaleTokens = scrapScaleValueTokens(tokens);
    if (scaleTokens.isEmpty()) {
        return std::nullopt;
    }

    ScrapScaleDefinition definition;
    definition.numbers.reserve(8);
    for (const QString &token : scaleTokens) {
        qreal parsedValue = 0.0;
        if (parseScaleNumber(token, &parsedValue)) {
            definition.numbers.append(parsedValue);
            continue;
        }

        if (definition.unitToken.isEmpty()) {
            definition.unitToken = normalizedScaleUnitToken(token);
        }
    }

    const std::optional<qreal> metersPerUnit = metersPerScaleUnit(definition.unitToken);
    if (!metersPerUnit.has_value()) {
        return std::nullopt;
    }

    if (definition.numbers.size() == 1) {
        definition.metersPerSourceUnit = definition.numbers.at(0) * metersPerUnit.value();
        definition.valid = definition.metersPerSourceUnit > 1e-9;
    } else if (definition.numbers.size() == 2 && !definition.unitToken.isEmpty()) {
        const qreal drawingUnits = definition.numbers.at(0);
        const qreal realLength = definition.numbers.at(1);
        if (std::fabs(drawingUnits) > 1e-9 && std::fabs(realLength) > 1e-9) {
            definition.metersPerSourceUnit = (realLength / drawingUnits) * metersPerUnit.value();
            definition.valid = definition.metersPerSourceUnit > 1e-9;
        }
    } else if (definition.numbers.size() >= 8) {
        const QPointF sourcePoint1(definition.numbers.at(0), definition.numbers.at(1));
        const QPointF sourcePoint2(definition.numbers.at(2), definition.numbers.at(3));
        const QPointF scalePoint1(definition.numbers.at(4), definition.numbers.at(5));
        const QPointF scalePoint2(definition.numbers.at(6), definition.numbers.at(7));
        const qreal sourceLength = QLineF(sourcePoint1, sourcePoint2).length();
        const qreal scaleLength = QLineF(scalePoint1, scalePoint2).length();
        if (sourceLength > 1e-6 && scaleLength > 1e-6) {
            definition.metersPerSourceUnit = (scaleLength * metersPerUnit.value()) / sourceLength;
            definition.valid = definition.metersPerSourceUnit > 1e-9;
        }
    }

    if (!definition.valid) {
        return std::nullopt;
    }
    return definition;
}

enum class LineVertexRole
{
    None,
    Anchor,
    IncomingControl,
    OutgoingControl
};

class MapCardMoveCommand final : public QUndoCommand
{
public:
    MapCardMoveCommand(MapCardItem *item, const QPointF &oldPosition, const QPointF &newPosition)
        : item_(item)
        , oldPosition_(oldPosition)
        , newPosition_(newPosition)
    {
    }

    void undo() override
    {
        if (item_ != nullptr) {
            item_->setPos(oldPosition_);
        }
    }

    void redo() override
    {
        if (item_ != nullptr) {
            item_->setPos(newPosition_);
        }
    }

private:
    MapCardItem *item_ = nullptr;
    QPointF oldPosition_;
    QPointF newPosition_;
};

class MapCardVisibilityCommand final : public QUndoCommand
{
public:
    MapCardVisibilityCommand(MapCardItem *item, bool oldVisible, bool newVisible)
        : item_(item)
        , oldVisible_(oldVisible)
        , newVisible_(newVisible)
    {
    }

    void undo() override
    {
        if (item_ != nullptr) {
            item_->setObjectVisible(oldVisible_);
        }
    }

    void redo() override
    {
        if (item_ != nullptr) {
            item_->setObjectVisible(newVisible_);
        }
    }

private:
    MapCardItem *item_ = nullptr;
    bool oldVisible_ = true;
    bool newVisible_ = true;
};

class MapDraftMoveCommand final : public QUndoCommand
{
public:
    MapDraftMoveCommand(MapDraftGeometryItem *item, const QPointF &oldPosition, const QPointF &newPosition)
        : item_(item)
        , oldPosition_(oldPosition)
        , newPosition_(newPosition)
    {
    }

    void undo() override
    {
        if (item_ != nullptr) {
            item_->setPos(oldPosition_);
        }
    }

    void redo() override
    {
        if (item_ != nullptr) {
            item_->setPos(newPosition_);
        }
    }

private:
    MapDraftGeometryItem *item_ = nullptr;
    QPointF oldPosition_;
    QPointF newPosition_;
};

class MapDraftVisibilityCommand final : public QUndoCommand
{
public:
    MapDraftVisibilityCommand(MapDraftGeometryItem *item, bool oldVisible, bool newVisible)
        : item_(item)
        , oldVisible_(oldVisible)
        , newVisible_(newVisible)
    {
    }

    void undo() override
    {
        if (item_ != nullptr) {
            item_->setObjectVisible(oldVisible_);
        }
    }

    void redo() override
    {
        if (item_ != nullptr) {
            item_->setObjectVisible(newVisible_);
        }
    }

private:
    MapDraftGeometryItem *item_ = nullptr;
    bool oldVisible_ = true;
    bool newVisible_ = true;
};

class MapDraftAddCommand final : public QUndoCommand
{
public:
    MapDraftAddCommand(QGraphicsScene *scene, QVector<QGraphicsRectItem *> *items, MapDraftGeometryItem *item, const QPointF &position)
        : scene_(scene)
        , items_(items)
        , item_(item)
        , position_(position)
    {
    }

    void undo() override
    {
        if (scene_ != nullptr && item_ != nullptr) {
            scene_->removeItem(item_);
        }
        if (items_ != nullptr && item_ != nullptr) {
            items_->removeAll(item_);
        }
    }

    void redo() override
    {
        if (scene_ != nullptr && item_ != nullptr) {
            if (item_->scene() != scene_) {
                scene_->addItem(item_);
            }
            item_->setPos(position_);
            item_->setZValue(20.0);
            item_->setSelected(true);
        }
        if (items_ != nullptr && item_ != nullptr && !items_->contains(item_)) {
            items_->append(item_);
        }
    }

private:
    QGraphicsScene *scene_ = nullptr;
    QVector<QGraphicsRectItem *> *items_ = nullptr;
    MapDraftGeometryItem *item_ = nullptr;
    QPointF position_;
};

QColor draftGeometryAccent(DraftGeometryKind kind)
{
    switch (kind) {
    case DraftGeometryKind::Point:
        return QColor(QStringLiteral("#72d7ff"));
    case DraftGeometryKind::Line:
        return QColor(QStringLiteral("#ffb15a"));
    case DraftGeometryKind::Area:
        return QColor(QStringLiteral("#ff7f8f"));
    }

    return QColor(QStringLiteral("#72d7ff"));
}

QRectF draftGeometryBounds(DraftGeometryKind kind)
{
    switch (kind) {
    case DraftGeometryKind::Point:
        return QRectF(0, 0, 220, 120);
    case DraftGeometryKind::Line:
        return QRectF(0, 0, 260, 120);
    case DraftGeometryKind::Area:
        return QRectF(0, 0, 280, 140);
    }

    return QRectF(0, 0, 220, 120);
}

} // namespace

QRectF sceneCoordsPreviewBounds(const QRectF &sourceBounds, const QRectF &targetBounds)
{
    if (!sourceBounds.isValid() || !targetBounds.isValid()) {
        return targetBounds;
    }

    const qreal sourceWidth = qMax(1.0, sourceBounds.width());
    const qreal sourceHeight = qMax(1.0, sourceBounds.height());
    const qreal targetWidth = qMax(1.0, targetBounds.width());
    const qreal targetHeight = qMax(1.0, targetBounds.height());
    const qreal scale = qMin(targetWidth / sourceWidth, targetHeight / sourceHeight);
    const qreal fittedWidth = sourceWidth * scale;
    const qreal fittedHeight = sourceHeight * scale;
    const qreal left = targetBounds.left() + ((targetWidth - fittedWidth) / 2.0);
    const qreal top = targetBounds.top() + ((targetHeight - fittedHeight) / 2.0);
    return QRectF(left, top, fittedWidth, fittedHeight);
}

std::optional<QPointF> nearestWallGeometrySnapPoint(const QGraphicsScene *scene,
                                                   const QPointF &candidate,
                                                   qreal radius,
                                                   int excludedLineNumber)
{
    if (scene == nullptr || radius <= 0.0) {
        return std::nullopt;
    }
    // Hidden selection-gated anchors are tracked by the handle index too.
    const QVector<QGraphicsItem *> items = MapEditorSceneHandleIndex::itemsInRect(
        scene, QRectF(candidate.x() - radius, candidate.y() - radius, radius * 2.0, radius * 2.0));
    qreal bestDistanceSquared = radius * radius;
    std::optional<QPointF> bestPoint;
    auto consider = [&](const QPointF &point) {
        const QPointF delta = point - candidate;
        const qreal distanceSquared = QPointF::dotProduct(delta, delta);
        if (distanceSquared <= bestDistanceSquared
            && (!bestPoint || distanceSquared < bestDistanceSquared)) {
            bestDistanceSquared = distanceSquared;
            bestPoint = point;
        }
    };
    for (QGraphicsItem *item : items) {
        if (!isMapSceneWallItem(item)
            || item->data(kMapSceneLineNumberRole).toInt() == excludedLineNumber) {
            continue;
        }
        auto *vertex = dynamic_cast<MapEditableGeometryVertexItem *>(item);
        if (vertex != nullptr
            && item->data(kMapSceneSelectionSubtypeRole).toInt() == kMapSceneSelectionSubtypeLineAnchor) {
            consider(vertex->scenePos());
        }
    }
    // Retain exact wall anchors before considering a point along a wall segment.
    // Selection-gated hidden anchors remain valid geometry, as in ordinary snapping.
    if (bestPoint) {
        return bestPoint;
    }
    const QRectF probe(candidate.x() - radius, candidate.y() - radius, radius * 2.0, radius * 2.0);
    for (QGraphicsItem *item : scene->items(probe, Qt::IntersectsItemBoundingRect)) {
        if (!isMapSceneWallItem(item) || !item->isVisible()
            || item->data(kMapSceneLineNumberRole).toInt() == excludedLineNumber) {
            continue;
        }
        auto *path = dynamic_cast<QGraphicsPathItem *>(item);
        const int subtype = item->data(kMapSceneSelectionSubtypeRole).toInt();
        if (path == nullptr || (subtype != kMapSceneSelectionSubtypeGeneric
                                && subtype != kMapSceneSelectionSubtypeLineDetail)) {
            continue;
        }
        const qreal flattenScale = qMax<qreal>(1.0, 64.0 / radius);
        const QTransform flattenTransform = path->sceneTransform()
            * QTransform::fromScale(flattenScale, flattenScale);
        for (const QPolygonF &polygon : path->path().toSubpathPolygons(flattenTransform)) {
            for (int index = 1; index < polygon.size(); ++index) {
                const QPointF start = polygon.at(index - 1) / flattenScale;
                const QPointF direction = polygon.at(index) / flattenScale - start;
                const qreal lengthSquared = QPointF::dotProduct(direction, direction);
                const qreal ratio = lengthSquared > 0.0
                    ? std::clamp(QPointF::dotProduct(candidate - start, direction) / lengthSquared, 0.0, 1.0)
                    : 0.0;
                consider(start + direction * ratio);
            }
        }
    }
    return bestPoint;
}

QPointF sceneCoordsSourceToPreview(const QPointF &source, const QRectF &sourceBounds, const QRectF &previewBounds)
{
    const QRectF fitted = sceneCoordsPreviewBounds(sourceBounds, previewBounds);
    const qreal zoom = qMin(fitted.width() / qMax(1.0, sourceBounds.width()),
                            fitted.height() / qMax(1.0, sourceBounds.height()));
    const qreal panX = fitted.left() - (sourceBounds.left() * zoom);
    const qreal panY = fitted.top() + (sourceBounds.bottom() * zoom);
    return QPointF((source.x() * zoom) + panX,
                   panY - (source.y() * zoom));
}

QPointF sceneCoordsPreviewToSource(const QPointF &preview, const QRectF &sourceBounds, const QRectF &previewBounds)
{
    const QRectF fitted = sceneCoordsPreviewBounds(sourceBounds, previewBounds);
    const qreal zoom = qMin(fitted.width() / qMax(1.0, sourceBounds.width()),
                            fitted.height() / qMax(1.0, sourceBounds.height()));
    const qreal panX = fitted.left() - (sourceBounds.left() * zoom);
    const qreal panY = fitted.top() + (sourceBounds.bottom() * zoom);
    return QPointF((preview.x() - panX) / qMax(1e-6, zoom),
                   (panY - preview.y()) / qMax(1e-6, zoom));
}

qreal sceneCoordsScaleFactor(const QRectF &sourceBounds, const QRectF &previewBounds)
{
    if (!sourceBounds.isValid() || !previewBounds.isValid()) {
        return 1.0;
    }

    const qreal sourceWidth = qMax(1.0, sourceBounds.width());
    const qreal sourceHeight = qMax(1.0, sourceBounds.height());
    const qreal targetWidth = qMax(1.0, previewBounds.width());
    const qreal targetHeight = qMax(1.0, previewBounds.height());
    return qMin(targetWidth / sourceWidth, targetHeight / sourceHeight);
}

std::optional<qreal> sourceUnitsPerMeterFromScrapScale(const QStringList &tokens)
{
    const std::optional<ScrapScaleDefinition> definition = parseScrapScaleDefinition(tokens);
    if (!definition.has_value()) {
        return std::nullopt;
    }
    return 1.0 / definition->metersPerSourceUnit;
}

CoordinateTransform coordinateTransformFromScrapScale(const QStringList &tokens)
{
    CoordinateTransform transform;

    const std::optional<ScrapScaleDefinition> definition = parseScrapScaleDefinition(tokens);
    if (!definition.has_value()) {
        return transform;
    }

    if (definition->numbers.size() < 8) {
        const qreal scale = definition->metersPerSourceUnit;
        const QTransform sourceToMap(scale, 0.0, 0.0, scale, 0.0, 0.0);
        bool invertible = false;
        const QTransform mapToSource = sourceToMap.inverted(&invertible);
        if (!invertible) {
            return transform;
        }
        transform.valid = true;
        transform.sourceToMap = sourceToMap;
        transform.mapToSource = mapToSource;
        return transform;
    }

    const std::optional<qreal> metersPerUnit = metersPerScaleUnit(definition->unitToken);
    if (!metersPerUnit.has_value()) {
        return transform;
    }

    const QVector<qreal> &numbers = definition->numbers;
    const QPointF sourcePoint1(numbers.at(0), numbers.at(1));
    const QPointF sourcePoint2(numbers.at(2), numbers.at(3));
    const QPointF mapPoint1(numbers.at(4) * metersPerUnit.value(), numbers.at(5) * metersPerUnit.value());
    const QPointF mapPoint2(numbers.at(6) * metersPerUnit.value(), numbers.at(7) * metersPerUnit.value());

    const QPointF sourceVector = sourcePoint2 - sourcePoint1;
    const QPointF mapVector = mapPoint2 - mapPoint1;
    const qreal sourceLength = std::hypot(sourceVector.x(), sourceVector.y());
    const qreal mapLength = std::hypot(mapVector.x(), mapVector.y());
    if (sourceLength < 1e-6 || mapLength < 1e-6) {
        return transform;
    }

    const qreal scale = mapLength / sourceLength;
    const qreal cosTheta = ((sourceVector.x() * mapVector.x()) + (sourceVector.y() * mapVector.y())) / (sourceLength * mapLength);
    const qreal sinTheta = ((sourceVector.x() * mapVector.y()) - (sourceVector.y() * mapVector.x())) / (sourceLength * mapLength);
    const qreal a = scale * cosTheta;
    const qreal b = scale * sinTheta;
    const qreal c = -scale * sinTheta;
    const qreal d = scale * cosTheta;
    const qreal tx = mapPoint1.x() - (a * sourcePoint1.x()) - (c * sourcePoint1.y());
    const qreal ty = mapPoint1.y() - (b * sourcePoint1.x()) - (d * sourcePoint1.y());

    const QTransform sourceToMap(a, b, c, d, tx, ty);
    bool invertible = false;
    const QTransform mapToSource = sourceToMap.inverted(&invertible);
    if (!invertible) {
        return transform;
    }

    transform.valid = true;
    transform.sourceToMap = sourceToMap;
    transform.mapToSource = mapToSource;
    return transform;
}

QVector<QPointF> pointsFromTokens(const QStringList &tokens)
{
    QVector<QPointF> points;
    QVector<qreal> numericValues;
    numericValues.reserve(tokens.size());

    for (const QString &token : tokens) {
        if (!TherionTokenRules::isNumericToken(token)) {
            continue;
        }

        bool ok = false;
        const qreal numericValue = token.toDouble(&ok);
        if (ok) {
            numericValues.append(numericValue);
        }
    }

    for (int index = 0; index + 1 < numericValues.size(); index += 2) {
        points.append(QPointF(numericValues.at(index), numericValues.at(index + 1)));
    }

    return points;
}

MapCardItem::MapCardItem(const MapSceneEntry &entry, const QColor &accent)
    : lineNumber_(entry.lineNumber)
    , category_(entry.category)
    , title_(entry.title)
    , subtitle_(entry.subtitle)
    , accent_(accent)
{
    setRect(QRectF(0, 0, 260, 100));
    setPen(Qt::NoPen);
    setBrush(Qt::NoBrush);
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    setFlag(QGraphicsItem::ItemIsMovable, true);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
    setAcceptHoverEvents(true);
    setCursor(Qt::PointingHandCursor);
}

int MapCardItem::lineNumber() const
{
    return lineNumber_;
}

QString MapCardItem::category() const
{
    return category_;
}

bool MapCardItem::isObjectVisible() const
{
    return objectVisible_;
}

void MapCardItem::setObjectVisible(bool visible)
{
    if (objectVisible_ == visible) {
        return;
    }

    objectVisible_ = visible;
    setOpacity(objectVisible_ ? 1.0 : 0.45);
    update();
}

void MapCardItem::setMoveCommittedCallback(std::function<void(int, const QPointF &, const QPointF &)> callback)
{
    moveCommittedCallback_ = std::move(callback);
}

void MapCardItem::setVisibilityCommittedCallback(std::function<void(int, bool, bool)> callback)
{
    visibilityCommittedCallback_ = std::move(callback);
}

void MapCardItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(widget);

    const bool selected = option->state & QStyle::State_Selected;
    const bool hovered = option->state & QStyle::State_MouseOver;
    const QColor backgroundColor = selected ? accent_.lighter(170) : (hovered ? QColor(QStringLiteral("#313747")) : QColor(QStringLiteral("#2a2f3a")));
    const QColor borderColor = selected ? accent_.lighter(120) : (hovered ? accent_.lighter(110) : accent_.darker(160));
    const QColor titleColor = selected ? QColor(QStringLiteral("#f8fbff")) : QColor(QStringLiteral("#e8edf4"));
    const QColor bodyColor = selected ? QColor(QStringLiteral("#f0f4fb")) : QColor(QStringLiteral("#bac5d3"));
    const QColor badgeColor = selected ? accent_.lighter(150) : (hovered ? accent_.lighter(115) : accent_);
    const QColor visibilityColor = objectVisible_ ? QColor(QStringLiteral("#6ed8a8")) : QColor(QStringLiteral("#f08a7f"));

    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(QPen(borderColor, selected ? 2.5 : (hovered ? 1.9 : 1.4)));
    painter->setBrush(backgroundColor);
    painter->drawRoundedRect(rect().adjusted(1, 1, -1, -1), 10, 10);

    painter->setPen(Qt::NoPen);
    painter->setBrush(badgeColor);
    painter->drawRoundedRect(QRectF(rect().left() + 12, rect().top() + 12, 72, 22), 8, 8);

    QFont badgeFont = painter->font();
    badgeFont.setPointSize(badgeFont.pointSize() - 1);
    badgeFont.setBold(true);
    painter->setFont(badgeFont);
    painter->setPen(QColor(QStringLiteral("#111318")));
    painter->drawText(QRectF(rect().left() + 12, rect().top() + 12, 72, 22), Qt::AlignCenter, category_);

    painter->setBrush(visibilityColor);
    painter->setPen(Qt::NoPen);
    painter->drawEllipse(QRectF(rect().right() - 26, rect().top() + 16, 12, 12));
    QFont visibilityFont = painter->font();
    visibilityFont.setPointSize(visibilityFont.pointSize() - 4);
    visibilityFont.setBold(true);
    painter->setFont(visibilityFont);
    painter->setPen(QColor(QStringLiteral("#dfe6ef")));
    painter->drawText(QRectF(rect().right() - 68, rect().top() + 8, 34, 20), Qt::AlignRight | Qt::AlignVCenter, objectVisible_ ? QObject::tr("On") : QObject::tr("Off"));

    QFont titleFont = painter->font();
    titleFont.setPointSize(titleFont.pointSize() + 3);
    titleFont.setBold(true);
    painter->setFont(titleFont);
    painter->setPen(titleColor);
    painter->drawText(QRectF(rect().left() + 12, rect().top() + 42, rect().width() - 72, 28), Qt::AlignLeft | Qt::AlignVCenter, title_);

    QFont subtitleFont = painter->font();
    subtitleFont.setPointSize(subtitleFont.pointSize() - 1);
    subtitleFont.setBold(false);
    painter->setFont(subtitleFont);
    painter->setPen(bodyColor);
    painter->drawText(QRectF(rect().left() + 12, rect().top() + 68, rect().width() - 24, 18), Qt::AlignLeft | Qt::AlignVCenter, subtitle_);

    QFont lineFont = painter->font();
    lineFont.setPointSize(lineFont.pointSize() - 2);
    painter->setFont(lineFont);
    painter->setPen(QColor(QStringLiteral("#9ca7b6")));
    painter->drawText(QRectF(rect().right() - 84, rect().top() + 12, 72, 22), Qt::AlignRight | Qt::AlignVCenter, QObject::tr("L%1").arg(lineNumber_));
}

void MapCardItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && visibilityHotspot().contains(event->pos())) {
        const bool previousVisible = objectVisible_;
        setObjectVisible(!objectVisible_);
        if (visibilityCommittedCallback_) {
            visibilityCommittedCallback_(lineNumber_, previousVisible, objectVisible_);
        }
        event->accept();
        return;
    }

    pressPosition_ = pos();
    QGraphicsRectItem::mousePressEvent(event);
}

void MapCardItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsRectItem::mouseReleaseEvent(event);

    if (moveCommittedCallback_ && pressPosition_ != pos()) {
        moveCommittedCallback_(lineNumber_, pressPosition_, pos());
    }
}

QVariant MapCardItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == QGraphicsItem::ItemPositionChange && scene() != nullptr) {
        QPointF point = value.toPointF();
        point.setX(qBound(56.0, point.x(), scene()->sceneRect().right() - rect().width() - 56.0));
        point.setY(qMax(point.y(), 132.0));
        return point;
    }

    return QGraphicsRectItem::itemChange(change, value);
}

QRectF MapCardItem::visibilityHotspot() const
{
    return QRectF(rect().right() - 74, rect().top() + 8, 56, 24);
}

QString draftGeometryLabel(DraftGeometryKind kind)
{
    switch (kind) {
    case DraftGeometryKind::Point:
        return QObject::tr("Point");
    case DraftGeometryKind::Line:
        return QObject::tr("Line");
    case DraftGeometryKind::Area:
        return QObject::tr("Area");
    }

    return QObject::tr("Point");
}

MapDraftGeometryItem::MapDraftGeometryItem(int draftId, DraftGeometryKind kind)
    : draftId_(draftId)
    , kind_(kind)
    , accent_(draftGeometryAccent(kind))
    , label_(draftGeometryLabel(kind))
{
    setRect(draftGeometryBounds(kind));
    setPen(Qt::NoPen);
    setBrush(Qt::NoBrush);
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    setFlag(QGraphicsItem::ItemIsMovable, true);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
    setAcceptHoverEvents(true);
    setCursor(Qt::PointingHandCursor);
}

int MapDraftGeometryItem::draftId() const
{
    return draftId_;
}

DraftGeometryKind MapDraftGeometryItem::kind() const
{
    return kind_;
}

bool MapDraftGeometryItem::isDraftComplete() const
{
    return draftComplete_;
}

void MapDraftGeometryItem::setDraftComplete(bool complete)
{
    if (draftComplete_ == complete) {
        return;
    }

    draftComplete_ = complete;
    update();
}

bool MapDraftGeometryItem::isObjectVisible() const
{
    return objectVisible_;
}

void MapDraftGeometryItem::setObjectVisible(bool visible)
{
    if (objectVisible_ == visible) {
        return;
    }

    objectVisible_ = visible;
    setOpacity(objectVisible_ ? 1.0 : 0.45);
    update();
}

void MapDraftGeometryItem::setMoveCommittedCallback(std::function<void(int, const QPointF &, const QPointF &)> callback)
{
    moveCommittedCallback_ = std::move(callback);
}

void MapDraftGeometryItem::setVisibilityCommittedCallback(std::function<void(int, bool, bool)> callback)
{
    visibilityCommittedCallback_ = std::move(callback);
}

void MapDraftGeometryItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(widget);

    const bool selected = option->state & QStyle::State_Selected;
    const bool hovered = option->state & QStyle::State_MouseOver;
    const QColor borderColor = selected ? accent_.lighter(120) : (hovered ? accent_.lighter(110) : accent_.darker(150));
    const QColor fillColor = draftComplete_ ? accent_.lighter(150) : (hovered ? QColor(QStringLiteral("#343a45")) : QColor(QStringLiteral("#2b2f38")));
    const QColor titleColor = selected ? QColor(QStringLiteral("#ffffff")) : QColor(QStringLiteral("#edf3fb"));
    const QColor bodyColor = selected ? QColor(QStringLiteral("#eef5ff")) : QColor(QStringLiteral("#b9c4d3"));
    const QColor completionColor = draftComplete_ ? QColor(QStringLiteral("#6ed8a8")) : QColor(QStringLiteral("#f2c65f"));

    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(QPen(borderColor, selected ? 2.2 : (hovered ? 1.9 : 1.4), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter->setBrush(fillColor);
    painter->drawRoundedRect(rect().adjusted(1, 1, -1, -1), 12, 12);

    painter->setPen(Qt::NoPen);
    painter->setBrush(completionColor);
    painter->drawRoundedRect(QRectF(rect().left() + 12, rect().top() + 12, 84, 22), 8, 8);

    QFont badgeFont = painter->font();
    badgeFont.setPointSize(badgeFont.pointSize() - 1);
    badgeFont.setBold(true);
    painter->setFont(badgeFont);
    painter->setPen(QColor(QStringLiteral("#13161b")));
    painter->drawText(QRectF(rect().left() + 12, rect().top() + 12, 84, 22), Qt::AlignCenter, draftComplete_ ? QObject::tr("Complete") : QObject::tr("Draft"));

    painter->setPen(titleColor);
    QFont titleFont = painter->font();
    titleFont.setPointSize(titleFont.pointSize() + 2);
    titleFont.setBold(true);
    painter->setFont(titleFont);
    painter->drawText(QRectF(rect().left() + 12, rect().top() + 42, rect().width() - 24, 24), Qt::AlignLeft | Qt::AlignVCenter, label_);

    painter->setPen(bodyColor);
    QFont bodyFont = painter->font();
    bodyFont.setPointSize(bodyFont.pointSize() - 1);
    bodyFont.setBold(false);
    painter->setFont(bodyFont);
    painter->drawText(QRectF(rect().left() + 12, rect().top() + 66, rect().width() - 24, 20), Qt::AlignLeft | Qt::AlignVCenter, geometryDescription());

    painter->setBrush(objectVisible_ ? QColor(QStringLiteral("#6ed8a8")) : QColor(QStringLiteral("#f08a7f")));
    painter->setPen(Qt::NoPen);
    painter->drawEllipse(QRectF(rect().right() - 26, rect().top() + 16, 12, 12));
}

void MapDraftGeometryItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && visibilityHotspot().contains(event->pos())) {
        const bool previousVisible = objectVisible_;
        setObjectVisible(!objectVisible_);
        if (visibilityCommittedCallback_) {
            visibilityCommittedCallback_(draftId_, previousVisible, objectVisible_);
        }
        event->accept();
        return;
    }

    pressPosition_ = pos();
    QGraphicsRectItem::mousePressEvent(event);
}

void MapDraftGeometryItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsRectItem::mouseReleaseEvent(event);

    if (moveCommittedCallback_ && pressPosition_ != pos()) {
        moveCommittedCallback_(draftId_, pressPosition_, pos());
    }
}

QVariant MapDraftGeometryItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == QGraphicsItem::ItemPositionChange && scene() != nullptr) {
        QPointF point = value.toPointF();
        point.setX(qBound(56.0, point.x(), scene()->sceneRect().right() - rect().width() - 56.0));
        point.setY(qMax(point.y(), 132.0));
        return point;
    }

    return QGraphicsRectItem::itemChange(change, value);
}

QRectF MapDraftGeometryItem::visibilityHotspot() const
{
    return QRectF(rect().right() - 74, rect().top() + 8, 56, 24);
}

QString MapDraftGeometryItem::geometryDescription() const
{
    switch (kind_) {
    case DraftGeometryKind::Point:
        return QObject::tr("Draft point anchor");
    case DraftGeometryKind::Line:
        return QObject::tr("Draft line segment");
    case DraftGeometryKind::Area:
        return QObject::tr("Draft area outline");
    }

    return QString();
}

QRectF geometryBoundsForFeatures(const QVector<MapGeometryFeature> &features)
{
    QRectF bounds;
    bool hasBounds = false;

    auto includePoint = [&bounds, &hasBounds](const QPointF &point) {
        if (!hasBounds) {
            bounds = QRectF(point, QSizeF(1.0, 1.0));
            hasBounds = true;
            return;
        }

        bounds = bounds.united(QRectF(point, QSizeF(1.0, 1.0)));
    };

    for (const MapGeometryFeature &feature : features) {
        if (feature.hasAnchor) {
            includePoint(feature.anchor);
        }

        for (const QPointF &vertex : feature.vertices) {
            includePoint(vertex);
        }

        for (const MapGeometryFeature::TH2LineVertex &lineVertex : feature.lineVertices) {
            includePoint(lineVertex.anchor);
            if (lineVertex.incomingControl.has_value()) {
                includePoint(lineVertex.incomingControl.value());
            }
            if (lineVertex.outgoingControl.has_value()) {
                includePoint(lineVertex.outgoingControl.value());
            }
        }
    }

    if (!hasBounds) {
        return QRectF(0.0, 0.0, 1.0, 1.0);
    }

    return bounds;
}

QPointF mapGeometryPointToPreview(const QPointF &point, const QRectF &sourceBounds, const QRectF &targetBounds)
{
    return sceneCoordsSourceToPreview(point, sourceBounds, targetBounds);
}

QPointF mapGeometryPreviewToSource(const QPointF &point, const QRectF &sourceBounds, const QRectF &targetBounds)
{
    return sceneCoordsPreviewToSource(point, sourceBounds, targetBounds);
}

std::optional<QPointF> mirroredSmoothControlPoint(const QPointF &anchor,
                                                  const QPointF &movedControlPoint,
                                                  const std::optional<QPointF> &oppositeControlPoint)
{
    constexpr qreal kEpsilon = 1e-6;

    const QPointF vector = movedControlPoint - anchor;
    const qreal vectorLength = std::hypot(vector.x(), vector.y());
    if (vectorLength <= kEpsilon) {
        return std::nullopt;
    }

    const QPointF direction(vector.x() / vectorLength, vector.y() / vectorLength);
    const qreal oppositeLength = oppositeControlPoint.has_value()
        ? std::hypot(oppositeControlPoint->x() - anchor.x(), oppositeControlPoint->y() - anchor.y())
        : vectorLength;
    return QPointF(anchor.x() - (direction.x() * oppositeLength),
                   anchor.y() - (direction.y() * oppositeLength));
}

QVector<MapLineSecondaryMove> collectLineSecondaryMovesForVertexDrag(const MapGeometryFeature &lineFeature,
                                                                     int sourceVertexIndex,
                                                                     const QPointF &oldPoint,
                                                                     const QPointF &newPoint)
{
    QVector<MapLineSecondaryMove> moves;
    if (sourceVertexIndex < 0 || lineFeature.lineVertices.isEmpty()) {
        return moves;
    }

    auto appendMove = [&moves](int moveSourceVertexIndex, const QPointF &from, const QPointF &to) {
        if (moveSourceVertexIndex < 0 || !mapSourcePointsDiffer(from, to)) {
            return;
        }

        MapLineSecondaryMove move;
        move.sourceVertexIndex = moveSourceVertexIndex;
        move.oldPoint = from;
        move.newPoint = to;
        moves.append(move);
    };

    for (const MapGeometryFeature::TH2LineVertex &vertex : lineFeature.lineVertices) {
        if (vertex.anchorSourceVertexIndex == sourceVertexIndex) {
            const QPointF delta = newPoint - oldPoint;
            if (!delta.isNull()) {
                if (vertex.incomingSourceVertexIndex >= 0) {
                    const QPointF oldIncoming = vertex.incomingControl.value_or(vertex.anchor);
                    appendMove(vertex.incomingSourceVertexIndex, oldIncoming, oldIncoming + delta);
                }
                if (vertex.outgoingSourceVertexIndex >= 0) {
                    const QPointF oldOutgoing = vertex.outgoingControl.value_or(vertex.anchor);
                    appendMove(vertex.outgoingSourceVertexIndex, oldOutgoing, oldOutgoing + delta);
                }
            }
            return moves;
        }

        LineVertexRole role = LineVertexRole::None;
        if (vertex.incomingSourceVertexIndex == sourceVertexIndex) {
            role = LineVertexRole::IncomingControl;
        } else if (vertex.outgoingSourceVertexIndex == sourceVertexIndex) {
            role = LineVertexRole::OutgoingControl;
        }
        if (role == LineVertexRole::None || !vertex.isSmooth) {
            continue;
        }

        const QPointF anchor = vertex.anchor;
        if (role == LineVertexRole::IncomingControl) {
            if (vertex.outgoingSourceVertexIndex < 0) {
                return moves;
            }

            const QPointF oldOpposite = vertex.outgoingControl.value_or(anchor);
            const std::optional<QPointF> newOpposite = mirroredSmoothControlPoint(anchor, newPoint, vertex.outgoingControl);
            if (newOpposite.has_value()) {
                appendMove(vertex.outgoingSourceVertexIndex, oldOpposite, newOpposite.value());
            }
            return moves;
        }

        if (role == LineVertexRole::OutgoingControl) {
            if (vertex.incomingSourceVertexIndex < 0) {
                return moves;
            }

            const QPointF oldOpposite = vertex.incomingControl.value_or(anchor);
            const std::optional<QPointF> newOpposite = mirroredSmoothControlPoint(anchor, newPoint, vertex.incomingControl);
            if (newOpposite.has_value()) {
                appendMove(vertex.incomingSourceVertexIndex, oldOpposite, newOpposite.value());
            }
            return moves;
        }
    }

    return moves;
}

QVector<MapLineSecondaryMove> collectLinePreviewSecondaryMovesForVertexDrag(const MapGeometryFeature &lineFeature,
                                                                            int sourceVertexIndex,
                                                                            const QPointF &oldPoint,
                                                                            const QPointF &newPoint)
{
    QVector<MapLineSecondaryMove> moves = collectLineSecondaryMovesForVertexDrag(lineFeature,
                                                                                  sourceVertexIndex,
                                                                                  oldPoint,
                                                                                  newPoint);
    QVector<MapLineSecondaryMove> filtered;
    filtered.reserve(moves.size());
    for (const MapLineSecondaryMove &move : std::as_const(moves)) {
        if (move.sourceVertexIndex == sourceVertexIndex) {
            continue;
        }
        filtered.append(move);
    }
    return filtered;
}

QVector<MapLineSecondaryMove> collectLinePreviewCoupledUpdatesForVertexDrag(const MapGeometryFeature &lineFeature,
                                                                            int sourceVertexIndex,
                                                                            const QPointF &oldPoint,
                                                                            const QPointF &newPoint,
                                                                            const QHash<int, QPointF> &currentControlPoints)
{
    QVector<MapLineSecondaryMove> updates;
    if (sourceVertexIndex < 0 || lineFeature.lineVertices.isEmpty()) {
        return updates;
    }

    for (const MapGeometryFeature::TH2LineVertex &vertex : lineFeature.lineVertices) {
        if (vertex.anchorSourceVertexIndex != sourceVertexIndex) {
            continue;
        }

        const QPointF delta = newPoint - oldPoint;
        if (delta.isNull()) {
            return updates;
        }

        auto appendAnchorTransport = [&](int controlSourceVertexIndex, const QPointF &fallbackBasePoint) {
            if (controlSourceVertexIndex < 0) {
                return;
            }
            MapLineSecondaryMove update;
            update.sourceVertexIndex = controlSourceVertexIndex;
            update.oldPoint = currentControlPoints.value(controlSourceVertexIndex, fallbackBasePoint);
            update.newPoint = update.oldPoint + delta;
            if (mapSourcePointsDiffer(update.oldPoint, update.newPoint)) {
                updates.append(update);
            }
        };

        appendAnchorTransport(vertex.incomingSourceVertexIndex,
                              vertex.incomingControl.value_or(vertex.anchor));
        appendAnchorTransport(vertex.outgoingSourceVertexIndex,
                              vertex.outgoingControl.value_or(vertex.anchor));
        return updates;
    }

    return collectLinePreviewSecondaryMovesForVertexDrag(lineFeature,
                                                         sourceVertexIndex,
                                                         oldPoint,
                                                         newPoint);
}

bool insertLineVertexByDeCasteljau(QVector<MapGeometryFeature::TH2LineVertex> *lineVertices,
                                   int segmentStartIndex,
                                   qreal t,
                                   int *insertedVertexIndex)
{
    if (lineVertices == nullptr || lineVertices->size() < 2) {
        return false;
    }
    if (segmentStartIndex < 0 || segmentStartIndex + 1 >= lineVertices->size()) {
        return false;
    }

    const qreal clampedT = qBound(0.0, t, 1.0);
    MapGeometryFeature::TH2LineVertex &startVertex = (*lineVertices)[segmentStartIndex];
    MapGeometryFeature::TH2LineVertex &endVertex = (*lineVertices)[segmentStartIndex + 1];

    auto lerp = [clampedT](const QPointF &a, const QPointF &b) {
        return QPointF(a.x() + ((b.x() - a.x()) * clampedT),
                       a.y() + ((b.y() - a.y()) * clampedT));
    };
    auto normalizeControl = [](const QPointF &anchor, const QPointF &control) -> std::optional<QPointF> {
        return mapSourcePointsDiffer(anchor, control) ? std::optional<QPointF>(control) : std::nullopt;
    };

    const QPointF p0 = startVertex.anchor;
    const QPointF p1 = startVertex.outgoingControl.value_or(startVertex.anchor);
    const QPointF p2 = endVertex.incomingControl.value_or(endVertex.anchor);
    const QPointF p3 = endVertex.anchor;
    const bool segmentIsCubic = startVertex.outgoingControl.has_value() || endVertex.incomingControl.has_value();

    MapGeometryFeature::TH2LineVertex inserted;
    inserted.anchorSourceVertexIndex = -1;
    inserted.incomingSourceVertexIndex = -1;
    inserted.outgoingSourceVertexIndex = -1;
    inserted.isSmooth = true;

    if (!segmentIsCubic) {
        inserted.anchor = lerp(p0, p3);
        inserted.incomingControl.reset();
        inserted.outgoingControl.reset();
        startVertex.outgoingControl.reset();
        endVertex.incomingControl.reset();
    } else {
        const QPointF p01 = lerp(p0, p1);
        const QPointF p12 = lerp(p1, p2);
        const QPointF p23 = lerp(p2, p3);
        const QPointF p012 = lerp(p01, p12);
        const QPointF p123 = lerp(p12, p23);
        const QPointF p0123 = lerp(p012, p123);

        inserted.anchor = p0123;
        inserted.incomingControl = normalizeControl(inserted.anchor, p012);
        inserted.outgoingControl = normalizeControl(inserted.anchor, p123);
        startVertex.outgoingControl = normalizeControl(startVertex.anchor, p01);
        endVertex.incomingControl = normalizeControl(endVertex.anchor, p23);
    }

    lineVertices->insert(segmentStartIndex + 1, inserted);
    if (insertedVertexIndex != nullptr) {
        *insertedVertexIndex = segmentStartIndex + 1;
    }

    return true;
}

bool removeLineVertexWithReconnect(QVector<MapGeometryFeature::TH2LineVertex> *lineVertices,
                                   int vertexIndex)
{
    if (lineVertices == nullptr || lineVertices->size() < 3) {
        return false;
    }
    if (vertexIndex <= 0 || vertexIndex >= lineVertices->size() - 1) {
        return false;
    }

    const MapGeometryFeature::TH2LineVertex removed = lineVertices->at(vertexIndex);
    MapGeometryFeature::TH2LineVertex &previous = (*lineVertices)[vertexIndex - 1];
    MapGeometryFeature::TH2LineVertex &next = (*lineVertices)[vertexIndex + 1];

    const bool reconnectThroughIncoming = removed.incomingControl.has_value();
    const bool reconnectThroughOutgoing = removed.outgoingControl.has_value();
    previous.outgoingControl = reconnectThroughIncoming
        ? removed.incomingControl
        : std::nullopt;
    next.incomingControl = reconnectThroughOutgoing
        ? removed.outgoingControl
        : std::nullopt;

    lineVertices->removeAt(vertexIndex);
    return true;
}

QUndoCommand *createMapCardMoveCommand(MapCardItem *item, const QPointF &oldPosition, const QPointF &newPosition)
{
    return new MapCardMoveCommand(item, oldPosition, newPosition);
}

QUndoCommand *createMapCardVisibilityCommand(MapCardItem *item, bool oldVisible, bool newVisible)
{
    return new MapCardVisibilityCommand(item, oldVisible, newVisible);
}

QUndoCommand *createMapDraftMoveCommand(MapDraftGeometryItem *item, const QPointF &oldPosition, const QPointF &newPosition)
{
    return new MapDraftMoveCommand(item, oldPosition, newPosition);
}

QUndoCommand *createMapDraftVisibilityCommand(MapDraftGeometryItem *item, bool oldVisible, bool newVisible)
{
    return new MapDraftVisibilityCommand(item, oldVisible, newVisible);
}

QUndoCommand *createMapDraftAddCommand(QGraphicsScene *scene,
                                       QVector<QGraphicsRectItem *> *items,
                                       MapDraftGeometryItem *item,
                                       const QPointF &position)
{
    return new MapDraftAddCommand(scene, items, item, position);
}

} // namespace TherionStudio
