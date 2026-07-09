#pragma once

#include <QColor>
#include <QHash>
#include <QGraphicsItem>
#include <QGraphicsRectItem>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QTransform>
#include <QVariant>
#include <QVector>

#include <functional>
#include <optional>

class QGraphicsScene;
class QGraphicsSceneMouseEvent;
class QPainter;
class QStyleOptionGraphicsItem;
class QUndoCommand;
class QWidget;

namespace TherionStudio
{
class Th2GeometryProjection;
struct TherionSourceLogicalCommand;

constexpr int kMapItemRole = Qt::UserRole + 120;
constexpr int kMapItemGeometryValue = 1;
constexpr int kMapSceneLineNumberRole = Qt::UserRole + 121;
constexpr int kMapSceneSelectionGatedRole = Qt::UserRole + 122;
constexpr int kMapSceneSelectionSubtypeRole = Qt::UserRole + 123;
constexpr int kMapSceneOwnerVertexRole = Qt::UserRole + 124;
constexpr int kMapSceneInteractionHoverRole = Qt::UserRole + 125;
constexpr int kMapScenePendingPrimarySelectionRole = Qt::UserRole + 126;
constexpr int kMapSceneInteractionSelectionRole = Qt::UserRole + 127;
constexpr int kMapSceneFocusedVertexRole = Qt::UserRole + 128;

enum MapSceneSelectionSubtype
{
    kMapSceneSelectionSubtypeGeneric = 0,
    kMapSceneSelectionSubtypeLineDetail = 1,
    kMapSceneSelectionSubtypeLineAnchor = 2,
    kMapSceneSelectionSubtypeLineControl = 3,
    kMapSceneSelectionSubtypeLineControlConnector = 4,
    kMapSceneSelectionSubtypeAreaVertex = 5,
    kMapSceneSelectionSubtypePointOrientationHandle = 6,
    kMapSceneSelectionSubtypeAreaFill = 7
};

struct TherionParsedLine;

enum class DraftGeometryKind
{
    Point,
    Line,
    Area
};

enum class MapGeometryAreaPlace
{
    Bottom,
    Default,
    Top
};

struct MapSceneEntry
{
    int lineNumber = 0;
    QString category;
    QString title;
    QString subtitle;
};

struct MapGeometryFeature
{
    enum class Kind
    {
        Point,
        Line,
        Area
    };
    struct LineSegment
    {
        enum class Type
        {
            Linear,
            Cubic
        };

        Type type = Type::Linear;
        QString subtype;
        int endVertexIndex = -1;
        int control1VertexIndex = -1;
        int control2VertexIndex = -1;
    };
    struct TH2LineVertex
    {
        QPointF anchor;
        std::optional<QPointF> incomingControl;
        std::optional<QPointF> outgoingControl;
        std::optional<qreal> orientationDegrees;
        std::optional<qreal> leftSize;
        bool isSmooth = true;
        QStringList standaloneOptionRows;
        int anchorSourceVertexIndex = -1;
        int incomingSourceVertexIndex = -1;
        int outgoingSourceVertexIndex = -1;
    };

    Kind kind = Kind::Point;
    int lineNumber = 0;
    int scrapLineNumber = 0;
    QString category;
    QString label;
    QString subtype;
    QString subtitle;
    QHash<QString, QString> optionValues;
    QColor accent;
    QVector<QPointF> vertices;
    QPointF anchor;
    QPointF sourceAnchor;
    bool hasAnchor = false;
    bool hasSourceAnchor = false;
    bool orientationSupported = false;
    std::optional<qreal> orientationDegrees;
    bool closed = false;
    bool clipToScrap = true;
    MapGeometryAreaPlace areaPlace = MapGeometryAreaPlace::Default;
    bool reversed = false;
    bool stationPoint = false;
    bool verticesEditable = true;
    bool hasCoordinateTransform = false;
    QTransform sourceToMapTransform;
    QTransform mapToSourceTransform;
    QVector<LineSegment> lineSegments;
    QVector<TH2LineVertex> lineVertices;
};

struct MapLineSecondaryMove
{
    int sourceVertexIndex = -1;
    QPointF oldPoint;
    QPointF newPoint;
};

struct MapGeometryItemGroupRemovalResult
{
    int removedItems = 0;
    int removedVertexIndexEntries = 0;
    bool removedPrimaryItem = false;
};

struct MapGeometryItemGroupRenderResult
{
    int addedItems = 0;
    int addedVertexIndexEntries = 0;
    bool addedPrimaryItem = false;
};

struct CoordinateTransform
{
    bool valid = false;
    QTransform sourceToMap;
    QTransform mapToSource;
};

class MapCardItem final : public QGraphicsRectItem
{
public:
    MapCardItem(const MapSceneEntry &entry, const QColor &accent);

    void setMoveCommittedCallback(std::function<void(int, const QPointF &, const QPointF &)> callback);
    void setVisibilityCommittedCallback(std::function<void(int, bool, bool)> callback);

    int lineNumber() const;
    QString category() const;
    bool isObjectVisible() const;
    void setObjectVisible(bool visible);

protected:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

private:
    QRectF visibilityHotspot() const;

    int lineNumber_ = 0;
    QString category_;
    QString title_;
    QString subtitle_;
    QColor accent_;
    bool objectVisible_ = true;
    QPointF pressPosition_;
    std::function<void(int, const QPointF &, const QPointF &)> moveCommittedCallback_;
    std::function<void(int, bool, bool)> visibilityCommittedCallback_;
};

class MapDraftGeometryItem final : public QGraphicsRectItem
{
public:
    MapDraftGeometryItem(int draftId, DraftGeometryKind kind);

    int draftId() const;
    DraftGeometryKind kind() const;

    bool isDraftComplete() const;
    void setDraftComplete(bool complete);

    bool isObjectVisible() const;
    void setObjectVisible(bool visible);

    void setMoveCommittedCallback(std::function<void(int, const QPointF &, const QPointF &)> callback);
    void setVisibilityCommittedCallback(std::function<void(int, bool, bool)> callback);

protected:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

private:
    QRectF visibilityHotspot() const;
    QString geometryDescription() const;

    int draftId_ = 0;
    DraftGeometryKind kind_ = DraftGeometryKind::Point;
    QColor accent_;
    QString label_;
    bool draftComplete_ = false;
    bool objectVisible_ = true;
    QPointF pressPosition_;
    std::function<void(int, const QPointF &, const QPointF &)> moveCommittedCallback_;
    std::function<void(int, bool, bool)> visibilityCommittedCallback_;
};

QString draftGeometryLabel(DraftGeometryKind kind);
QString mapWorkspaceHelpHtml();

QString mapEntryCategoryForLine(const TherionParsedLine &parsedLine);
QString mapEntryTitleForLine(const TherionParsedLine &parsedLine);
QString mapEntrySubtitleForLine(const TherionParsedLine &parsedLine);
QColor mapEntryAccentForCategory(const QString &category);
QVector<MapSceneEntry> collectMapSceneEntries(const QVector<TherionParsedLine> &parsedLines);

QRectF geometryBoundsForFeatures(const QVector<MapGeometryFeature> &features);
std::optional<qreal> sourceUnitsPerMeterFromScrapScale(const QStringList &tokens);
CoordinateTransform coordinateTransformFromScrapScale(const QStringList &tokens);
QPointF mapGeometryPointToPreview(const QPointF &point, const QRectF &sourceBounds, const QRectF &targetBounds);
QPointF mapGeometryPreviewToSource(const QPointF &point, const QRectF &sourceBounds, const QRectF &targetBounds);
QVector<MapGeometryFeature> collectGeometryFeatures(const QVector<TherionParsedLine> &parsedLines);
QVector<MapGeometryFeature> collectGeometryFeatures(const Th2GeometryProjection &projection,
                                                    const QVector<TherionSourceLogicalCommand> &commands);
bool insertLineVertexByDeCasteljau(QVector<MapGeometryFeature::TH2LineVertex> *lineVertices,
                                   int segmentStartIndex,
                                   qreal t,
                                   int *insertedVertexIndex = nullptr);
bool removeLineVertexWithReconnect(QVector<MapGeometryFeature::TH2LineVertex> *lineVertices,
                                   int vertexIndex);
std::optional<QPointF> mirroredSmoothControlPoint(const QPointF &anchor,
                                                  const QPointF &movedControlPoint,
                                                  const std::optional<QPointF> &oppositeControlPoint);
QVector<MapLineSecondaryMove> collectLineSecondaryMovesForVertexDrag(const MapGeometryFeature &lineFeature,
                                                                     int sourceVertexIndex,
                                                                     const QPointF &oldPoint,
                                                                     const QPointF &newPoint);
QVector<MapLineSecondaryMove> collectLinePreviewSecondaryMovesForVertexDrag(const MapGeometryFeature &lineFeature,
                                                                            int sourceVertexIndex,
                                                                            const QPointF &oldPoint,
                                                                            const QPointF &newPoint);
QVector<MapLineSecondaryMove> collectLinePreviewCoupledUpdatesForVertexDrag(const MapGeometryFeature &lineFeature,
                                                                            int sourceVertexIndex,
                                                                            const QPointF &oldPoint,
                                                                            const QPointF &newPoint,
                                                                            const QHash<int, QPointF> &currentControlPoints);
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
                             const std::function<void(int, int, qreal, qreal)> &recordLinePointLeftHandleChange);
MapGeometryItemGroupRemovalResult removeMapGeometryItemGroupForLine(QGraphicsScene *scene,
                                                                    int lineNumber,
                                                                    QHash<int, QGraphicsItem *> *mapItemsByLine,
                                                                    QHash<QString, QGraphicsItem *> *mapVertexItemsByKey);
MapGeometryItemGroupRenderResult renderMapGeometryItemGroupForFeature(
    QGraphicsScene *scene,
    const MapGeometryFeature &feature,
    const QRectF &sourceBounds,
    QHash<int, QGraphicsItem *> *mapItemsByLine,
    QHash<QString, QGraphicsItem *> *mapVertexItemsByKey,
    const std::function<void(int, const QPointF &, const QPointF &)> &recordPointGeometryMove,
    const std::function<void(int, const QString &, int, const QPointF &, const QPointF &)> &recordLineAreaVertexMove,
    const std::function<void(int, qreal)> &recordPointOrientationHandleChange,
    const std::function<void(int, int, qreal, qreal)> &recordLinePointLeftHandleChange);

QUndoCommand *createMapCardMoveCommand(MapCardItem *item, const QPointF &oldPosition, const QPointF &newPosition);
QUndoCommand *createMapCardVisibilityCommand(MapCardItem *item, bool oldVisible, bool newVisible);
QUndoCommand *createMapDraftMoveCommand(MapDraftGeometryItem *item, const QPointF &oldPosition, const QPointF &newPosition);
QUndoCommand *createMapDraftVisibilityCommand(MapDraftGeometryItem *item, bool oldVisible, bool newVisible);
QUndoCommand *createMapDraftAddCommand(QGraphicsScene *scene,
                                       QVector<QGraphicsRectItem *> *items,
                                       MapDraftGeometryItem *item,
                                       const QPointF &position);

} // namespace TherionStudio
