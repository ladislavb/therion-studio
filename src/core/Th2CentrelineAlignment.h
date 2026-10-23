#pragma once

#include <QPointF>
#include <QHash>
#include <QString>
#include <QVector>

#include <optional>

namespace TherionStudio
{
struct TherionSourceLogicalCommand;
struct TherionXviDocument;

// 2D similarity without reflection: p' = [a -b; b a] p + (tx, ty).
struct Th2SimilarityTransform
{
    qreal a = 1.0;
    qreal b = 0.0;
    qreal tx = 0.0;
    qreal ty = 0.0;

    [[nodiscard]] static Th2SimilarityTransform fromScale(qreal scale);
    [[nodiscard]] static Th2SimilarityTransform fromTranslation(const QPointF &offset);
    [[nodiscard]] QPointF map(const QPointF &point) const;
    [[nodiscard]] Th2SimilarityTransform inverted() const;
    // Applies this transform, then `next`.
    [[nodiscard]] Th2SimilarityTransform then(const Th2SimilarityTransform &next) const;
    [[nodiscard]] qreal scale() const;
};

// A station position computed by Therion: X east, Y north, Z up, in metres.
struct Th2StationPosition
{
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct Th2ScrapStationPoint
{
    QString stationName;
    QPointF position;
    int lineNumber = 0;
};

struct Th2StationFitPair
{
    QString stationName;
    QPointF drawingPosition;
    QPointF centrelineMetres;
};

// A surveyed leg as measured, before loop closure: X east, Y north, Z up, in metres.
struct Th2MeasuredLeg
{
    QString fromStationKey;
    QString toStationKey;
    Th2StationPosition vector;
};

enum class Th2CentrelineFitSource
{
    None,
    // Measured legs between the scrap's station points: scale and rotation as drawn.
    MeasuredLegs,
    XviStations,
    // The XVI grid alone: scale and orientation of the background the scrap was drawn on.
    XviGrid,
};

// Similarity transform from scrap drawing coordinates to survey-local metres. Translation is only meaningful for
// XVI station fits; placements link two drawings at a station, so they never rely on it.
struct Th2CentrelineFit
{
    Th2CentrelineFitSource source = Th2CentrelineFitSource::None;
    Th2SimilarityTransform drawingToMetres;
    int stationCount = 0;
    qreal rmsResidualMetres = 0.0;

    [[nodiscard]] bool isValid() const { return source != Th2CentrelineFitSource::None; }
};

[[nodiscard]] std::optional<qreal> therionLengthUnitMetres(const QString &unit);

// Drawing position of a `point x y station` command.
[[nodiscard]] std::optional<QPointF> th2PointCommandPosition(const TherionSourceLogicalCommand &command);

// Station points of the scrap that starts at `scrapStartLine` (no scrap station-name transform applied).
[[nodiscard]] QVector<Th2ScrapStationPoint> th2ScrapStationPoints(const QVector<TherionSourceLogicalCommand> &commands,
                                                                  int scrapStartLine);

// Metres per drawing unit from the XVI grid (`XVIgrid` step length per `XVIgrids` real length).
[[nodiscard]] std::optional<qreal> xviMetresPerDrawingUnit(const TherionXviDocument &xvi);

// Pairs scrap station points with XVI centreline stations of the same name, in XVI metres.
[[nodiscard]] QVector<Th2StationFitPair> xviStationFitPairs(const QVector<Th2ScrapStationPoint> &points,
                                                            const TherionXviDocument &xvi);

// The XVI grid as a drawing-to-metres similarity: grid scale, and rotation so the grid's x axis points east.
[[nodiscard]] std::optional<Th2SimilarityTransform> xviGridTransform(const TherionXviDocument &xvi);

// Least-squares similarity fit of drawing positions onto centreline metres; needs two or more pairs.
[[nodiscard]] std::optional<Th2SimilarityTransform> similarityFit(const QVector<Th2StationFitPair> &pairs,
                                                      qreal *rmsResidual = nullptr);

// Two or more XVI pairs: fitted similarity. Otherwise the XVI grid.
[[nodiscard]] Th2CentrelineFit fitTh2ScrapToCentreline(const QVector<Th2StationFitPair> &xviPairs,
                                                       std::optional<Th2SimilarityTransform> xviGrid);

// Start line of the scrap that contains `lineNumber`, or 0 when the line is outside any scrap.
[[nodiscard]] int th2ScrapStartLineContaining(const QVector<TherionSourceLogicalCommand> &commands, int lineNumber);

// The `point ... station` command starting at `lineNumber`, if there is one inside a scrap.
[[nodiscard]] std::optional<Th2ScrapStationPoint> th2StationPointAtLine(
    const QVector<TherionSourceLogicalCommand> &commands,
    int lineNumber);

// Fits the scrap containing `lineNumber` to whichever XVI matches most of its station points, or to the first XVI
// grid when none match two. The scrap `-scale` is never used: it can differ in every scrap.
[[nodiscard]] Th2CentrelineFit fitTh2ScrapContainingLine(const QVector<TherionSourceLogicalCommand> &commands,
                                                         int lineNumber,
                                                         const QVector<TherionXviDocument> &xviDocuments);

// A Therion-computed station position or measured leg (X east, Y north, Z up, metres) seen in a scrap projection:
// `plan` gives (X, Y); `elevation θ` (degrees) gives (X cos θ - Y sin θ, Z). Other projections have no fixed frame.
[[nodiscard]] std::optional<QPointF> projectedStationMetres(const Th2StationPosition &position, const QString &projection);

// Fits the scrap containing `lineNumber` to the measured legs whose two stations it draws; `stationKeyByLine` gives
// the station each station point line draws. Fits scale and rotation, or in `extended` scale only (left or right
// as drawn). Left out are legs that look short in the projection, as a small drawing error turns them a lot, legs
// of a station drawn more than once in the scrap, and legs far off the median leg (a misplaced station point).
[[nodiscard]] Th2CentrelineFit fitTh2ScrapToMeasuredLegs(const QVector<TherionSourceLogicalCommand> &commands,
                                                         int lineNumber,
                                                         const QHash<int, QString> &stationKeyByLine,
                                                         const QVector<Th2MeasuredLeg> &legs,
                                                         const QString &projection);

// The same fit from a scrap's station points given as station key and drawing position.
[[nodiscard]] Th2CentrelineFit fitTh2StationPointsToMeasuredLegs(const QVector<QPair<QString, QPointF>> &drawnStations,
                                                                 const QVector<Th2MeasuredLeg> &legs,
                                                                 const QString &projection);

// True when two fits of one scrap differ in scale by more than a tenth (a scrap drawn off its XVI background).
// Rotation is not compared: it differs by the declination when the drawing was made without it.
[[nodiscard]] bool th2FitScalesDisagree(const Th2CentrelineFit &first, const Th2CentrelineFit &second);

// Where two drawings that share no station meet: a station point in each and the projected offset of the
// partner's station from this one, from computed positions.
struct Th2JoinAnchor
{
    QPointF thisPoint;
    QPointF partnerPoint;
    QPointF partnerOffsetMetres;
};

// Projected offset of `partnerStationKey` from `thisStationKey`, from computed positions.
[[nodiscard]] std::optional<QPointF> computedStationOffsetMetres(const QString &thisStationKey,
                                                                 const QString &partnerStationKey,
                                                                 const QHash<QString, Th2StationPosition> &positionsByStationKey,
                                                                 const QString &projection);

// The closest pair of station points, one in each scrap, by computed positions. The shortest offset is least
// affected by loop closure.
[[nodiscard]] std::optional<Th2JoinAnchor> closestComputedStationPair(const QVector<Th2ScrapStationPoint> &thisPoints,
                                                                      const QHash<int, QString> &thisStationKeyByLine,
                                                                      const QVector<Th2ScrapStationPoint> &partnerPoints,
                                                                      const QHash<int, QString> &partnerStationKeyByLine,
                                                                      const QHash<QString, Th2StationPosition> &positionsByStationKey,
                                                                      const QString &projection);

// Maps partner drawing coordinates into this drawing so the partner's join point lands `partnerOffsetMetres` from
// this drawing's join point (zero when the two are one station).
[[nodiscard]] Th2SimilarityTransform joinPartnerToThisTransform(const Th2CentrelineFit &thisFit,
                                                    const QPointF &thisJoinPoint,
                                                    const Th2CentrelineFit &partnerFit,
                                                    const QPointF &partnerJoinPoint,
                                                    const QPointF &partnerOffsetMetres = QPointF());
}
