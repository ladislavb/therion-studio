#include "Th2CentrelineAlignment.h"

#include "TherionSourceLogicalDocument.h"
#include "TherionStationNameRules.h"
#include "TherionXviParser.h"

#include <QHash>
#include <QLineF>
#include <QSet>
#include <QtMath>

#include <algorithm>

namespace TherionStudio
{
namespace
{
// Legs shorter than this in the projection, or than this share of the longest, are left out of a leg fit:
// a 0.1 m drawing error turns a 1 m leg by about 6 degrees.
constexpr qreal kMinimumLegMetres = 1.0;
constexpr qreal kMinimumLegShareOfLongest = 0.1;
// Legs off the median leg fit by more than this, and by more than three times the median, are left out as
// misdrawn.
constexpr qreal kMisdrawnLegMetres = 0.5;
constexpr qreal kMisdrawnLegShareOfMedian = 3.0;
// Scale difference above which two fits of one scrap disagree.
constexpr qreal kFitScaleTolerance = 0.1;

// A vector in drawing units and the same vector in metres.
struct VectorPair
{
    QPointF drawing;
    QPointF metres;
};

// Closed-form least squares for metres = s * R(theta) * drawing (or s * drawing without rotation).
std::optional<Th2SimilarityTransform> rotationScaleFit(const QVector<VectorPair> &vectors, bool withRotation)
{
    qreal dot = 0.0;
    qreal cross = 0.0;
    qreal drawingSpread = 0.0;
    for (const VectorPair &vector : vectors) {
        const QPointF &d = vector.drawing;
        const QPointF &m = vector.metres;
        dot += d.x() * m.x() + d.y() * m.y();
        cross += d.x() * m.y() - d.y() * m.x();
        drawingSpread += d.x() * d.x() + d.y() * d.y();
    }
    if (drawingSpread <= 0.0) {
        return std::nullopt;
    }
    return Th2SimilarityTransform{dot / drawingSpread, withRotation ? cross / drawingSpread : 0.0, 0.0, 0.0};
}

qreal median(QVector<qreal> values)
{
    std::sort(values.begin(), values.end());
    return values.at(values.size() / 2);
}

// The median of each leg's own scale and rotation: one misdrawn leg cannot pull it, unlike a least-squares fit.
Th2SimilarityTransform medianLegTransform(const QVector<VectorPair> &vectors, bool withRotation)
{
    QVector<qreal> as;
    QVector<qreal> bs;
    for (const VectorPair &vector : vectors) {
        const std::optional<Th2SimilarityTransform> own = rotationScaleFit({vector}, withRotation);
        as.append(own->a);
        bs.append(own->b);
    }
    return Th2SimilarityTransform{median(as), median(bs), 0.0, 0.0};
}

// Leaves out legs far off the median leg fit, as their station points are misplaced.
QVector<VectorPair> withoutMisdrawnLegs(const QVector<VectorPair> &vectors, bool withRotation)
{
    if (vectors.size() < 3) {
        return vectors;
    }
    const Th2SimilarityTransform typical = medianLegTransform(vectors, withRotation);
    QVector<qreal> residuals;
    for (const VectorPair &vector : vectors) {
        residuals.append(QLineF(typical.map(vector.drawing), vector.metres).length());
    }
    const qreal limit = qMax(kMisdrawnLegMetres, kMisdrawnLegShareOfMedian * median(residuals));
    QVector<VectorPair> kept;
    for (int index = 0; index < vectors.size(); ++index) {
        if (residuals.at(index) <= limit) {
            kept.append(vectors.at(index));
        }
    }
    return kept;
}

qreal vectorRmsResidual(const QVector<VectorPair> &vectors, const Th2SimilarityTransform &transform)
{
    qreal sum = 0.0;
    for (const VectorPair &vector : vectors) {
        const QPointF delta = transform.map(vector.drawing) - vector.metres;
        sum += delta.x() * delta.x() + delta.y() * delta.y();
    }
    return vectors.isEmpty() ? 0.0 : qSqrt(sum / vectors.size());
}

// A measured leg seen in a projection; in `extended` its horizontal length runs the way it is drawn.
std::optional<QPointF> projectedLegMetres(const Th2StationPosition &leg, const QString &projection, const QPointF &drawn)
{
    if (projection == QStringLiteral("extended")) {
        const qreal horizontal = qSqrt(leg.x * leg.x + leg.y * leg.y);
        return QPointF(drawn.x() < 0.0 ? -horizontal : horizontal, leg.z);
    }
    return projectedStationMetres(leg, projection);
}

std::optional<qreal> numberToken(const QString &token)
{
    bool ok = false;
    const qreal value = token.toDouble(&ok);
    return ok ? std::optional<qreal>(value) : std::nullopt;
}

QString bareStationName(const QString &name)
{
    return name.section(QLatin1Char('@'), 0, 0).trimmed();
}

}

Th2SimilarityTransform Th2SimilarityTransform::fromScale(qreal scale)
{
    return Th2SimilarityTransform{scale, 0.0, 0.0, 0.0};
}

Th2SimilarityTransform Th2SimilarityTransform::fromTranslation(const QPointF &offset)
{
    return Th2SimilarityTransform{1.0, 0.0, offset.x(), offset.y()};
}

QPointF Th2SimilarityTransform::map(const QPointF &point) const
{
    return QPointF(a * point.x() - b * point.y() + tx, b * point.x() + a * point.y() + ty);
}

Th2SimilarityTransform Th2SimilarityTransform::inverted() const
{
    const qreal determinant = a * a + b * b;
    if (determinant <= 0.0) {
        return Th2SimilarityTransform{};
    }
    Th2SimilarityTransform inverse{a / determinant, -b / determinant, 0.0, 0.0};
    const QPointF offset = inverse.map(QPointF(tx, ty));
    inverse.tx = -offset.x();
    inverse.ty = -offset.y();
    return inverse;
}

Th2SimilarityTransform Th2SimilarityTransform::then(const Th2SimilarityTransform &next) const
{
    const QPointF offset = next.map(QPointF(tx, ty));
    return Th2SimilarityTransform{next.a * a - next.b * b, next.a * b + next.b * a, offset.x(), offset.y()};
}

qreal Th2SimilarityTransform::scale() const
{
    return qSqrt(a * a + b * b);
}

std::optional<qreal> therionLengthUnitMetres(const QString &unit)
{
    static const QHash<QString, qreal> metresByUnit = {
        {QStringLiteral("m"), 1.0},          {QStringLiteral("meter"), 1.0},     {QStringLiteral("meters"), 1.0},
        {QStringLiteral("metre"), 1.0},      {QStringLiteral("metres"), 1.0},    {QStringLiteral("cm"), 0.01},
        {QStringLiteral("centimeter"), 0.01}, {QStringLiteral("centimeters"), 0.01},
        {QStringLiteral("centimetre"), 0.01}, {QStringLiteral("centimetres"), 0.01},
        {QStringLiteral("mm"), 0.001},       {QStringLiteral("millimeter"), 0.001},
        {QStringLiteral("millimeters"), 0.001}, {QStringLiteral("millimetre"), 0.001},
        {QStringLiteral("millimetres"), 0.001}, {QStringLiteral("in"), 0.0254},  {QStringLiteral("inch"), 0.0254},
        {QStringLiteral("inches"), 0.0254},  {QStringLiteral("ft"), 0.3048},    {QStringLiteral("feet"), 0.3048},
        {QStringLiteral("yd"), 0.9144},      {QStringLiteral("yard"), 0.9144},  {QStringLiteral("yards"), 0.9144},
    };
    const auto it = metresByUnit.constFind(unit.trimmed().toLower());
    return it == metresByUnit.constEnd() ? std::nullopt : std::optional<qreal>(it.value());
}

std::optional<QPointF> th2PointCommandPosition(const TherionSourceLogicalCommand &command)
{
    if (command.positionalArgumentRanges.size() < 2) {
        return std::nullopt;
    }
    const std::optional<qreal> x = numberToken(command.positionalArgumentRanges.at(0).text);
    const std::optional<qreal> y = numberToken(command.positionalArgumentRanges.at(1).text);
    return x.has_value() && y.has_value() ? std::optional<QPointF>(QPointF(*x, *y)) : std::nullopt;
}

QVector<Th2ScrapStationPoint> th2ScrapStationPoints(const QVector<TherionSourceLogicalCommand> &commands, int scrapStartLine)
{
    QVector<Th2ScrapStationPoint> points;
    bool inScrap = false;
    for (const TherionSourceLogicalCommand &command : commands) {
        const QString &commandName = command.metadata.commandName;
        if (commandName == QStringLiteral("scrap")) {
            inScrap = command.startLineNumber == scrapStartLine;
            continue;
        }
        if (!inScrap) {
            continue;
        }
        if (commandName == QStringLiteral("endscrap")) {
            break;
        }
        const std::optional<TherionSourceLogicalArgumentRange> name = therionStationPointNameRange(command);
        if (!name.has_value()) {
            continue;
        }
        const std::optional<QPointF> position = th2PointCommandPosition(command);
        if (position.has_value()) {
            points.append(Th2ScrapStationPoint{name->text.trimmed(), *position, command.startLineNumber});
        }
    }
    return points;
}

std::optional<qreal> xviMetresPerDrawingUnit(const TherionXviDocument &xvi)
{
    if (!xvi.hasGridSpacing) {
        return std::nullopt;
    }
    const qreal stepLength = QLineF(QPointF(), xvi.gridVectorX).length();
    const std::optional<qreal> unitMetres = therionLengthUnitMetres(xvi.gridSpacingUnit);
    if (stepLength <= 0.0 || !unitMetres.has_value()) {
        return std::nullopt;
    }
    return xvi.gridSpacing * *unitMetres / stepLength;
}

QVector<Th2StationFitPair> xviStationFitPairs(const QVector<Th2ScrapStationPoint> &points,
                                              const TherionXviDocument &xvi)
{
    QVector<Th2StationFitPair> pairs;
    const std::optional<qreal> metresPerUnit = xviMetresPerDrawingUnit(xvi);
    if (!metresPerUnit.has_value()) {
        return pairs;
    }
    QHash<QString, QPointF> xviStationsByName;
    for (const TherionXviStation &station : xvi.stationEntries) {
        // XTherion uses the first matching station name.
        if (!xviStationsByName.contains(bareStationName(station.name))) {
            xviStationsByName.insert(bareStationName(station.name), station.position);
        }
    }
    for (const Th2ScrapStationPoint &point : points) {
        const auto it = xviStationsByName.constFind(bareStationName(point.stationName));
        if (it != xviStationsByName.constEnd()) {
            pairs.append(Th2StationFitPair{point.stationName, point.position, it.value() * *metresPerUnit});
        }
    }
    return pairs;
}

std::optional<Th2SimilarityTransform> xviGridTransform(const TherionXviDocument &xvi)
{
    const std::optional<qreal> metresPerUnit = xviMetresPerDrawingUnit(xvi);
    if (!metresPerUnit.has_value()) {
        return std::nullopt;
    }
    const QPointF axis = xvi.gridVectorX / QLineF(QPointF(), xvi.gridVectorX).length();
    // Turn the grid's x axis onto east: rotate by minus its angle.
    return Th2SimilarityTransform{axis.x() * *metresPerUnit, -axis.y() * *metresPerUnit, 0.0, 0.0};
}

std::optional<Th2SimilarityTransform> similarityFit(const QVector<Th2StationFitPair> &pairs, qreal *rmsResidual)
{
    if (pairs.size() < 2) {
        return std::nullopt;
    }
    QPointF drawingCentroid;
    QPointF metresCentroid;
    for (const Th2StationFitPair &pair : pairs) {
        drawingCentroid += pair.drawingPosition;
        metresCentroid += pair.centrelineMetres;
    }
    drawingCentroid /= pairs.size();
    metresCentroid /= pairs.size();

    QVector<VectorPair> centred;
    for (const Th2StationFitPair &pair : pairs) {
        centred.append(VectorPair{pair.drawingPosition - drawingCentroid, pair.centrelineMetres - metresCentroid});
    }
    std::optional<Th2SimilarityTransform> fit = rotationScaleFit(centred, true);
    if (!fit.has_value()) {
        return std::nullopt;
    }
    if (rmsResidual != nullptr) {
        *rmsResidual = vectorRmsResidual(centred, *fit);
    }
    const QPointF t = metresCentroid - fit->map(drawingCentroid);
    fit->tx = t.x();
    fit->ty = t.y();
    return fit;
}

Th2CentrelineFit fitTh2ScrapToCentreline(const QVector<Th2StationFitPair> &xviPairs,
                                         std::optional<Th2SimilarityTransform> xviGrid)
{
    Th2CentrelineFit fit;
    qreal residual = 0.0;
    if (const std::optional<Th2SimilarityTransform> fitted = similarityFit(xviPairs, &residual)) {
        fit.source = Th2CentrelineFitSource::XviStations;
        fit.drawingToMetres = *fitted;
        fit.stationCount = xviPairs.size();
        fit.rmsResidualMetres = residual;
        return fit;
    }
    if (xviGrid.has_value()) {
        fit.source = Th2CentrelineFitSource::XviGrid;
        fit.drawingToMetres = *xviGrid;
        fit.stationCount = xviPairs.size();
    }
    return fit;
}

int th2ScrapStartLineContaining(const QVector<TherionSourceLogicalCommand> &commands, int lineNumber)
{
    int scrapStartLine = 0;
    for (const TherionSourceLogicalCommand &command : commands) {
        if (command.startLineNumber > lineNumber) {
            break;
        }
        if (command.metadata.commandName == QStringLiteral("scrap")) {
            scrapStartLine = command.startLineNumber;
        } else if (command.metadata.commandName == QStringLiteral("endscrap")) {
            scrapStartLine = 0;
        }
    }
    return scrapStartLine;
}

std::optional<Th2ScrapStationPoint> th2StationPointAtLine(const QVector<TherionSourceLogicalCommand> &commands,
                                                          int lineNumber)
{
    const int scrapStartLine = th2ScrapStartLineContaining(commands, lineNumber);
    if (scrapStartLine <= 0) {
        return std::nullopt;
    }
    for (const Th2ScrapStationPoint &point : th2ScrapStationPoints(commands, scrapStartLine)) {
        if (point.lineNumber == lineNumber) {
            return point;
        }
    }
    return std::nullopt;
}

Th2CentrelineFit fitTh2ScrapContainingLine(const QVector<TherionSourceLogicalCommand> &commands,
                                           int lineNumber,
                                           const QVector<TherionXviDocument> &xviDocuments)
{
    const int scrapStartLine = th2ScrapStartLineContaining(commands, lineNumber);
    if (scrapStartLine <= 0) {
        return Th2CentrelineFit();
    }
    const QVector<Th2ScrapStationPoint> points = th2ScrapStationPoints(commands, scrapStartLine);
    QVector<Th2StationFitPair> bestPairs;
    std::optional<Th2SimilarityTransform> bestGrid;
    for (const TherionXviDocument &xvi : xviDocuments) {
        const std::optional<Th2SimilarityTransform> grid = xviGridTransform(xvi);
        if (!grid.has_value()) {
            continue;
        }
        const QVector<Th2StationFitPair> pairs = xviStationFitPairs(points, xvi);
        if (!bestGrid.has_value() || pairs.size() > bestPairs.size()) {
            bestPairs = pairs;
            bestGrid = grid;
        }
    }
    return fitTh2ScrapToCentreline(bestPairs, bestGrid);
}

Th2SimilarityTransform joinPartnerToThisTransform(const Th2CentrelineFit &thisFit,
                                      const QPointF &thisJoinPoint,
                                      const Th2CentrelineFit &partnerFit,
                                      const QPointF &partnerJoinPoint,
                                      const QPointF &partnerOffsetMetres)
{
    // Each drawing's metres frame has its own origin, so the frames are linked at the join points.
    const QPointF joinOffset = thisFit.drawingToMetres.map(thisJoinPoint) + partnerOffsetMetres
        - partnerFit.drawingToMetres.map(partnerJoinPoint);
    return partnerFit.drawingToMetres.then(Th2SimilarityTransform::fromTranslation(joinOffset))
        .then(thisFit.drawingToMetres.inverted());
}

std::optional<QPointF> projectedStationMetres(const Th2StationPosition &position, const QString &projection)
{
    if (projection == QStringLiteral("plan")) {
        return QPointF(position.x, position.y);
    }
    const QStringList parts = projection.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (parts.size() != 2 || parts.first() != QStringLiteral("elevation")) {
        return std::nullopt;
    }
    bool ok = false;
    const qreal angle = qDegreesToRadians(parts.at(1).toDouble(&ok));
    if (!ok) {
        return std::nullopt;
    }
    return QPointF(position.x * qCos(angle) - position.y * qSin(angle), position.z);
}

Th2CentrelineFit fitTh2ScrapToMeasuredLegs(const QVector<TherionSourceLogicalCommand> &commands,
                                           int lineNumber,
                                           const QHash<int, QString> &stationKeyByLine,
                                           const QVector<Th2MeasuredLeg> &legs,
                                           const QString &projection)
{
    const int scrapStartLine = th2ScrapStartLineContaining(commands, lineNumber);
    if (scrapStartLine <= 0) {
        return Th2CentrelineFit();
    }
    QVector<QPair<QString, QPointF>> drawnStations;
    for (const Th2ScrapStationPoint &point : th2ScrapStationPoints(commands, scrapStartLine)) {
        const QString key = stationKeyByLine.value(point.lineNumber);
        if (!key.isEmpty()) {
            drawnStations.append({key, point.position});
        }
    }
    return fitTh2StationPointsToMeasuredLegs(drawnStations, legs, projection);
}

Th2CentrelineFit fitTh2StationPointsToMeasuredLegs(const QVector<QPair<QString, QPointF>> &drawnStations,
                                                   const QVector<Th2MeasuredLeg> &legs,
                                                   const QString &projection)
{
    // A station drawn more than once in the scrap is ambiguous, so its legs are left out.
    QHash<QString, QPointF> drawnByStationKey;
    QSet<QString> drawnTwice;
    for (const auto &[key, position] : drawnStations) {
        if (drawnByStationKey.contains(key)) {
            drawnTwice.insert(key);
        }
        drawnByStationKey.insert(key, position);
    }
    for (const QString &key : drawnTwice) {
        drawnByStationKey.remove(key);
    }
    QVector<VectorPair> vectors;
    qreal longest = 0.0;
    for (const Th2MeasuredLeg &leg : legs) {
        const auto from = drawnByStationKey.constFind(leg.fromStationKey);
        const auto to = drawnByStationKey.constFind(leg.toStationKey);
        if (from == drawnByStationKey.cend() || to == drawnByStationKey.cend()) {
            continue;
        }
        const QPointF drawn = *to - *from;
        const std::optional<QPointF> metres = projectedLegMetres(leg.vector, projection, drawn);
        if (!metres.has_value()) {
            return Th2CentrelineFit();
        }
        if (drawn.isNull()) {
            continue;
        }
        vectors.append(VectorPair{drawn, *metres});
        longest = qMax(longest, QLineF(QPointF(), *metres).length());
    }
    const qreal minimum = qMax(kMinimumLegMetres, longest * kMinimumLegShareOfLongest);
    vectors.erase(std::remove_if(vectors.begin(), vectors.end(),
                                 [minimum](const VectorPair &vector) {
                                     return QLineF(QPointF(), vector.metres).length() < minimum;
                                 }),
                  vectors.end());
    const bool withRotation = projection != QStringLiteral("extended");
    vectors = withoutMisdrawnLegs(vectors, withRotation);
    const std::optional<Th2SimilarityTransform> transform = rotationScaleFit(vectors, withRotation);
    Th2CentrelineFit fit;
    if (!transform.has_value()) {
        return fit;
    }
    fit.source = Th2CentrelineFitSource::MeasuredLegs;
    fit.drawingToMetres = *transform;
    fit.stationCount = static_cast<int>(vectors.size());
    fit.rmsResidualMetres = vectorRmsResidual(vectors, *transform);
    return fit;
}

bool th2FitScalesDisagree(const Th2CentrelineFit &first, const Th2CentrelineFit &second)
{
    const qreal firstScale = first.drawingToMetres.scale();
    const qreal secondScale = second.drawingToMetres.scale();
    return qAbs(firstScale - secondScale) > kFitScaleTolerance * qMax(firstScale, secondScale);
}

std::optional<QPointF> computedStationOffsetMetres(const QString &thisStationKey,
                                                   const QString &partnerStationKey,
                                                   const QHash<QString, Th2StationPosition> &positionsByStationKey,
                                                   const QString &projection)
{
    const auto thisPosition = positionsByStationKey.constFind(thisStationKey);
    const auto partnerPosition = positionsByStationKey.constFind(partnerStationKey);
    if (thisStationKey.isEmpty() || partnerStationKey.isEmpty() || thisPosition == positionsByStationKey.cend()
        || partnerPosition == positionsByStationKey.cend()) {
        return std::nullopt;
    }
    const std::optional<QPointF> thisMetres = projectedStationMetres(*thisPosition, projection);
    const std::optional<QPointF> partnerMetres = projectedStationMetres(*partnerPosition, projection);
    if (!thisMetres.has_value() || !partnerMetres.has_value()) {
        return std::nullopt;
    }
    return *partnerMetres - *thisMetres;
}

std::optional<Th2JoinAnchor> closestComputedStationPair(const QVector<Th2ScrapStationPoint> &thisPoints,
                                                        const QHash<int, QString> &thisStationKeyByLine,
                                                        const QVector<Th2ScrapStationPoint> &partnerPoints,
                                                        const QHash<int, QString> &partnerStationKeyByLine,
                                                        const QHash<QString, Th2StationPosition> &positionsByStationKey,
                                                        const QString &projection)
{
    std::optional<Th2JoinAnchor> closest;
    qreal closestLength = 0.0;
    for (const Th2ScrapStationPoint &thisPoint : thisPoints) {
        for (const Th2ScrapStationPoint &partnerPoint : partnerPoints) {
            const std::optional<QPointF> offset =
                computedStationOffsetMetres(thisStationKeyByLine.value(thisPoint.lineNumber),
                                            partnerStationKeyByLine.value(partnerPoint.lineNumber),
                                            positionsByStationKey,
                                            projection);
            if (!offset.has_value()) {
                continue;
            }
            const qreal length = QLineF(QPointF(), *offset).length();
            if (!closest.has_value() || length < closestLength) {
                closest = Th2JoinAnchor{thisPoint.position, partnerPoint.position, *offset};
                closestLength = length;
            }
        }
    }
    return closest;
}
}
