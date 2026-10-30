#include "TherionScrap3D.h"
#include "TherionTokenRules.h"

#include <QHash>
#include <QLineF>

#include <algorithm>
#include <cmath>

namespace TherionStudio
{

QPointF TherionScrapTransform::transformPoint(const QPointF &scrapPoint) const
{
    const double cosTheta = std::cos(rotation);
    const double sinTheta = std::sin(rotation);
    const double x = scrapPoint.x();
    const double y = scrapPoint.y();
    const double tx_out = scale * (cosTheta * x - sinTheta * y) + tx;
    const double ty_out = scale * (sinTheta * x + cosTheta * y) + ty;
    return QPointF(tx_out, ty_out);
}

double TherionScrapTransform::interpolateZ(const QPointF &scrapPoint,
                                          const QVector<TherionScrapAnchor> &matchedAnchors) const
{
    if (matchedAnchors.isEmpty()) {
        return 0.0;
    }
    if (matchedAnchors.size() == 1) {
        return matchedAnchors.first().centrelinePosition.z;
    }

    double sumWeights = 0.0;
    double sumZ = 0.0;
    for (const auto &anchor : matchedAnchors) {
        const double dx = scrapPoint.x() - anchor.scrapPoint.x();
        const double dy = scrapPoint.y() - anchor.scrapPoint.y();
        const double distSq = dx * dx + dy * dy;
        if (distSq < 1e-4) {
            return anchor.centrelinePosition.z;
        }
        const double weight = 1.0 / distSq;
        sumWeights += weight;
        sumZ += weight * anchor.centrelinePosition.z;
    }

    return sumWeights > 0.0 ? (sumZ / sumWeights) : matchedAnchors.first().centrelinePosition.z;
}

TherionScrapScale TherionScrap3D::parseScaleOption(const QStringList &tokens)
{
    TherionScrapScale result;
    if (tokens.isEmpty()) {
        return result;
    }

    // Clean tokens: remove brackets e.g. "[-128" -> "-128", "m]" -> "m"
    QStringList cleaned;
    for (const QString &rawToken : tokens) {
        QString token = rawToken.trimmed();
        while (token.startsWith(QLatin1Char('['))) {
            token.remove(0, 1);
        }
        while (token.endsWith(QLatin1Char(']'))) {
            token.chop(1);
        }
        token = token.trimmed();
        if (!token.isEmpty()) {
            cleaned.append(token);
        }
    }

    // Therion scrap scale format: [x1 y1 x2 y2 X1 Y1 X2 Y2 <unit>]
    // Minimum 8 numeric tokens:
    if (cleaned.size() < 8) {
        return result;
    }

    bool ok = true;
    const double x1 = cleaned.at(0).toDouble(&ok);
    if (!ok) return result;
    const double y1 = cleaned.at(1).toDouble(&ok);
    if (!ok) return result;
    const double x2 = cleaned.at(2).toDouble(&ok);
    if (!ok) return result;
    const double y2 = cleaned.at(3).toDouble(&ok);
    if (!ok) return result;
    const double X1 = cleaned.at(4).toDouble(&ok);
    if (!ok) return result;
    const double Y1 = cleaned.at(5).toDouble(&ok);
    if (!ok) return result;
    const double X2 = cleaned.at(6).toDouble(&ok);
    if (!ok) return result;
    const double Y2 = cleaned.at(7).toDouble(&ok);
    if (!ok) return result;

    double unitFactor = 1.0;
    if (cleaned.size() >= 9) {
        const QString unit = cleaned.at(8).toLower();
        if (unit == QLatin1String("ft") || unit == QLatin1String("feet")) {
            unitFactor = 0.3048;
        } else if (unit == QLatin1String("cm")) {
            unitFactor = 0.01;
        } else if (unit == QLatin1String("in") || unit == QLatin1String("inch")) {
            unitFactor = 0.0254;
        } else if (unit == QLatin1String("yd") || unit == QLatin1String("yard")) {
            unitFactor = 0.9144;
        }
    }

    const double scrapDist = std::hypot(x2 - x1, y2 - y1);
    const double worldDist = std::hypot(X2 - X1, Y2 - Y1) * unitFactor;

    if (scrapDist > 1e-4) {
        result.p1 = QPointF(x1, y1);
        result.p2 = QPointF(x2, y2);
        result.worldLengthMeters = worldDist;
        result.metersPerPoint = worldDist / scrapDist;
        result.valid = true;
    }

    return result;
}

QString TherionScrap3D::resolveStationKey(
    const QString &stationName,
    const QString &scrapSurveyPath,
    const QHash<QString, ThreeDViewerVec3> &stationPositions)
{
    if (stationName.isEmpty()) {
        return QString();
    }

    // 1. Direct exact match
    if (stationPositions.contains(stationName)) {
        return stationName;
    }

    // 2. Qualified with scrap survey path if available (name@path or name.path)
    if (!scrapSurveyPath.isEmpty()) {
        const QString qualified = stationName.contains(QLatin1Char('@'))
            ? QStringLiteral("%1.%2").arg(stationName, scrapSurveyPath)
            : QStringLiteral("%1@%2").arg(stationName, scrapSurveyPath);
        if (stationPositions.contains(qualified)) {
            return qualified;
        }
    }

    // 3. Match by suffix or prefix (e.g. "0@hp" matches "0@hp.1302")
    const QString nameAt = stationName.contains(QLatin1Char('@')) ? stationName : (stationName + QLatin1Char('@'));
    for (auto it = stationPositions.cbegin(); it != stationPositions.cend(); ++it) {
        const QString &key = it.key();
        if (key.startsWith(nameAt + QLatin1Char('.')) || key == stationName) {
            return key;
        }
    }

    // 4. Try matching station name ignoring survey prefix/suffix
    const QString baseName = stationName.section(QLatin1Char('@'), 0, 0);
    for (auto it = stationPositions.cbegin(); it != stationPositions.cend(); ++it) {
        const QString &key = it.key();
        if (key.section(QLatin1Char('@'), 0, 0) == baseName) {
            return key;
        }
    }

    return QString();
}

namespace
{
QVector<QPointF> polyline2DForLine(const Th2LineObject &line)
{
    QVector<QPointF> polyline;
    for (const Th2LinePointRow &row : line.pointRows) {
        if (row.coordinatePoints.isEmpty()) {
            continue;
        }
        if (polyline.isEmpty()) {
            polyline.append(row.coordinatePoints.first());
            continue;
        }
        if (row.coordinatePoints.size() == 1) {
            polyline.append(row.coordinatePoints.first());
            continue;
        }
        if (row.coordinatePoints.size() < 3) {
            continue;
        }

        const QPointF p0 = polyline.last();
        const QPointF p1 = row.coordinatePoints.at(0);
        const QPointF p2 = row.coordinatePoints.at(1);
        const QPointF p3 = row.coordinatePoints.at(2);
        constexpr int kSteps = 4;
        for (int step = 1; step <= kSteps; ++step) {
            const double t = double(step) / double(kSteps);
            const double u = 1.0 - t;
            const double uu = u * u;
            const double tt = t * t;
            polyline.append(QPointF(
                uu * u * p0.x() + 3.0 * uu * t * p1.x() + 3.0 * u * tt * p2.x() + tt * t * p3.x(),
                uu * u * p0.y() + 3.0 * uu * t * p1.y() + 3.0 * u * tt * p2.y() + tt * t * p3.y()));
        }
    }
    if (line.closed && polyline.size() > 2
        && QLineF(polyline.first(), polyline.last()).length() > 1e-9) {
        polyline.append(polyline.first());
    }
    return polyline;
}

QVector<QVector<QPointF>> closedWallRings(const QVector<QVector<QPointF>> &wallLines)
{
    constexpr double kEndpointTolerance = 1e-6;
    const auto samePoint = [=](const QPointF &left, const QPointF &right) {
        return QLineF(left, right).length() <= kEndpointTolerance;
    };
    QVector<bool> consumed(wallLines.size(), false);
    QVector<QVector<QPointF>> rings;
    for (qsizetype startIndex = 0; startIndex < wallLines.size(); ++startIndex) {
        if (consumed.at(startIndex) || wallLines.at(startIndex).size() < 2) {
            continue;
        }
        QVector<QPointF> chain = wallLines.at(startIndex);
        consumed[startIndex] = true;
        if (samePoint(chain.first(), chain.last())) {
            rings.append(chain);
            continue;
        }

        while (true) {
            if (samePoint(chain.last(), chain.first())) {
                rings.append(chain);
                break;
            }

            qsizetype matchIndex = -1;
            bool reverseMatch = false;
            bool ambiguousMatch = false;
            for (qsizetype candidateIndex = 0; candidateIndex < wallLines.size(); ++candidateIndex) {
                if (consumed.at(candidateIndex) || wallLines.at(candidateIndex).size() < 2) {
                    continue;
                }
                const QVector<QPointF> &candidate = wallLines.at(candidateIndex);
                const bool matchesFront = samePoint(chain.last(), candidate.first());
                const bool matchesBack = samePoint(chain.last(), candidate.last());
                if (matchesFront == matchesBack) {
                    if (matchesFront) {
                        ambiguousMatch = true;
                        break;
                    }
                    continue;
                }
                if (matchIndex >= 0) {
                    ambiguousMatch = true;
                    break;
                }
                matchIndex = candidateIndex;
                reverseMatch = matchesBack;
            }
            if (ambiguousMatch || matchIndex < 0) {
                break;
            }

            consumed[matchIndex] = true;
            QVector<QPointF> next = wallLines.at(matchIndex);
            if (reverseMatch) {
                std::reverse(next.begin(), next.end());
            }
            for (qsizetype pointIndex = 1; pointIndex < next.size(); ++pointIndex) {
                chain.append(next.at(pointIndex));
            }
        }
    }
    return rings;
}

double cross2D(const QPointF &a, const QPointF &b, const QPointF &c)
{
    return (b.x() - a.x()) * (c.y() - a.y()) - (b.y() - a.y()) * (c.x() - a.x());
}

bool pointInTriangle(const QPointF &point, const QPointF &a, const QPointF &b, const QPointF &c, double orientation)
{
    constexpr double kEpsilon = 1e-9;
    return cross2D(a, b, point) * orientation >= -kEpsilon
        && cross2D(b, c, point) * orientation >= -kEpsilon
        && cross2D(c, a, point) * orientation >= -kEpsilon;
}

QVector<std::array<int, 3>> triangulateSimpleRing(QVector<QPointF> ring)
{
    constexpr double kEpsilon = 1e-9;
    if (ring.size() > 1 && QLineF(ring.first(), ring.last()).length() <= kEpsilon) {
        ring.removeLast();
    }
    if (ring.size() < 3) {
        return {};
    }

    double signedArea = 0.0;
    for (qsizetype index = 0; index < ring.size(); ++index) {
        const QPointF &current = ring.at(index);
        const QPointF &next = ring.at((index + 1) % ring.size());
        signedArea += current.x() * next.y() - next.x() * current.y();
    }
    if (std::abs(signedArea) <= kEpsilon) {
        return {};
    }

    const double orientation = signedArea > 0.0 ? 1.0 : -1.0;
    QVector<int> remaining;
    remaining.reserve(ring.size());
    for (int index = 0; index < ring.size(); ++index) {
        remaining.append(index);
    }

    QVector<std::array<int, 3>> triangles;
    triangles.reserve(ring.size() - 2);
    while (remaining.size() > 3) {
        bool clippedEar = false;
        for (int index = 0; index < remaining.size(); ++index) {
            const int previous = remaining.at((index + remaining.size() - 1) % remaining.size());
            const int current = remaining.at(index);
            const int next = remaining.at((index + 1) % remaining.size());
            if (cross2D(ring.at(previous), ring.at(current), ring.at(next)) * orientation <= kEpsilon) {
                continue;
            }

            bool containsVertex = false;
            for (const int candidate : remaining) {
                if (candidate == previous || candidate == current || candidate == next) {
                    continue;
                }
                if (pointInTriangle(ring.at(candidate),
                                    ring.at(previous),
                                    ring.at(current),
                                    ring.at(next),
                                    orientation)) {
                    containsVertex = true;
                    break;
                }
            }
            if (containsVertex) {
                continue;
            }

            triangles.append({previous, current, next});
            remaining.removeAt(index);
            clippedEar = true;
            break;
        }
        if (!clippedEar) {
            return {};
        }
    }
    triangles.append({remaining.at(0), remaining.at(1), remaining.at(2)});
    return triangles;
}

double zOnMeshSurface(const ThreeDViewerMeshGroup &mesh, const QPointF &point, double fallbackZ)
{
    constexpr double kEpsilon = 1e-9;
    for (const auto &triangle : mesh.triangles) {
        const ThreeDViewerVec3 &a = mesh.vertices.at(qsizetype(triangle[0]));
        const ThreeDViewerVec3 &b = mesh.vertices.at(qsizetype(triangle[1]));
        const ThreeDViewerVec3 &c = mesh.vertices.at(qsizetype(triangle[2]));
        const QPointF a2(a.x, a.y);
        const QPointF b2(b.x, b.y);
        const QPointF c2(c.x, c.y);
        const double area = cross2D(a2, b2, c2);
        if (std::abs(area) <= kEpsilon) {
            continue;
        }

        const double weightA = cross2D(b2, c2, point) / area;
        const double weightB = cross2D(c2, a2, point) / area;
        const double weightC = cross2D(a2, b2, point) / area;
        if (weightA >= -kEpsilon && weightB >= -kEpsilon && weightC >= -kEpsilon) {
            return weightA * a.z + weightB * b.z + weightC * c.z;
        }
    }
    return fallbackZ;
}


void appendAreaMesh(ThreeDViewerMeshGroup *mesh,
                    const QVector<QPointF> &ring,
                    const TherionScrapTransform &transform,
                    const QVector<TherionScrapAnchor> &matchedAnchors)
{
    if (mesh == nullptr) {
        return;
    }
    QVector<QPointF> polygon = ring;
    if (polygon.size() > 1 && QLineF(polygon.first(), polygon.last()).length() <= 1e-9) {
        polygon.removeLast();
    }
    const QVector<std::array<int, 3>> triangles = triangulateSimpleRing(polygon);
    if (triangles.isEmpty()) {
        return;
    }

    const quint32 firstVertex = quint32(mesh->vertices.size());
    for (const QPointF &point : polygon) {
        const QPointF horizontal = transform.transformPoint(point);
        mesh->vertices.append({horizontal.x(), horizontal.y(), transform.interpolateZ(point, matchedAnchors)});
    }
    for (const auto &triangle : triangles) {
        mesh->triangles.append({firstVertex + quint32(triangle[0]),
                                firstVertex + quint32(triangle[1]),
                                firstVertex + quint32(triangle[2])});
    }
}

}

TherionScrapTransform TherionScrap3D::fitTransform(
    const QVector<TherionScrapAnchor> &anchors,
    const TherionScrapScale &scaleOption)
{
    TherionScrapTransform transform;
    transform.anchorCount = anchors.size();

    QVector<TherionScrapAnchor> matched;
    for (const auto &anchor : anchors) {
        if (anchor.matched) {
            matched.append(anchor);
        }
    }
    transform.matchedAnchorCount = matched.size();

    if (matched.isEmpty()) {
        return transform;
    }

    if (matched.size() == 1) {
        const auto &anchor = matched.first();
        transform.approximate = true;
        transform.scale = scaleOption.valid && scaleOption.metersPerPoint > 1e-9
            ? scaleOption.metersPerPoint
            : 0.0254;
        transform.rotation = 0.0;
        transform.tx = anchor.centrelinePosition.x - transform.scale * anchor.scrapPoint.x();
        transform.ty = anchor.centrelinePosition.y - transform.scale * anchor.scrapPoint.y();
        transform.valid = true;
        return transform;
    }

    // Procrustes 2D similarity fit for N >= 2 anchors
    const int N = matched.size();
    double mean_px = 0.0;
    double mean_py = 0.0;
    double mean_Px = 0.0;
    double mean_Py = 0.0;

    for (const auto &a : matched) {
        mean_px += a.scrapPoint.x();
        mean_py += a.scrapPoint.y();
        mean_Px += a.centrelinePosition.x;
        mean_Py += a.centrelinePosition.y;
    }
    mean_px /= N;
    mean_py /= N;
    mean_Px /= N;
    mean_Py /= N;

    double var_p = 0.0;
    double Sxx = 0.0;
    double Syy = 0.0;
    double Sxy = 0.0;
    double Syx = 0.0;

    for (const auto &a : matched) {
        const double ux = a.scrapPoint.x() - mean_px;
        const double uy = a.scrapPoint.y() - mean_py;
        const double Ux = a.centrelinePosition.x - mean_Px;
        const double Uy = a.centrelinePosition.y - mean_Py;

        var_p += (ux * ux + uy * uy);
        Sxx += ux * Ux;
        Syy += uy * Uy;
        Sxy += ux * Uy;
        Syx += uy * Ux;
    }

    if (var_p < 1e-6) {
        // Degenerate points: fallback to 1 anchor translation
        const auto &anchor = matched.first();
        transform.approximate = true;
        transform.scale = scaleOption.valid && scaleOption.metersPerPoint > 1e-9
            ? scaleOption.metersPerPoint
            : 0.0254;
        transform.rotation = 0.0;
        transform.tx = anchor.centrelinePosition.x - transform.scale * anchor.scrapPoint.x();
        transform.ty = anchor.centrelinePosition.y - transform.scale * anchor.scrapPoint.y();
        transform.valid = true;
        return transform;
    }

    const double a = Sxx + Syy;
    const double b = Sxy - Syx;
    const double theta = std::atan2(b, a);
    const double s = std::hypot(a, b) / var_p;
    if (!std::isfinite(s) || s <= 1e-9) {
        return transform;
    }

    const double cosTheta = std::cos(theta);
    const double sinTheta = std::sin(theta);

    transform.scale = s;
    transform.rotation = theta;
    transform.tx = mean_Px - s * (cosTheta * mean_px - sinTheta * mean_py);
    transform.ty = mean_Py - s * (sinTheta * mean_px + cosTheta * mean_py);
    transform.valid = true;

    // Calculate residuals
    double sumResidual = 0.0;
    double maxResidual = 0.0;
    for (const auto &anchor : matched) {
        const QPointF fitted = transform.transformPoint(anchor.scrapPoint);
        const double dx = fitted.x() - anchor.centrelinePosition.x;
        const double dy = fitted.y() - anchor.centrelinePosition.y;
        const double res = std::hypot(dx, dy);
        sumResidual += res;
        maxResidual = std::max(maxResidual, res);
    }

    transform.meanResidual = sumResidual / N;
    transform.maxResidual = maxResidual;

    return transform;
}

TherionScrap3DModel TherionScrap3D::projectScrap(
    const Th2GeometryProjection &th2Projection,
    const QString &scrapId,
    const QString &scrapSurveyPath,
    const QHash<QString, ThreeDViewerVec3> &stationPositions,
    quint32 meshId,
    quint32 surveyId,
    const TherionScrap3DOptions &options)
{
    TherionScrap3DModel model;
    model.scrapId = scrapId;
    model.surveyPath = scrapSurveyPath;

    // Find scrap block options
    TherionScrapScale scaleOption;
    Th2SourceRange scrapRange;
    for (const auto &block : th2Projection.blockObjects()) {
        const QString blockScrapId = !block.type.isEmpty() ? block.type : block.id;
        if (block.kind == Th2GeometryObjectKind::Scrap && (scrapId.isEmpty() || blockScrapId == scrapId)) {
            model.scrapId = blockScrapId;
            scrapRange = block.sourceRange;
            model.projection = block.optionsByName.value(QStringLiteral("projection")).join(QLatin1Char(' '));
            if (model.projection.isEmpty()) {
                model.projection = block.optionsByName.value(QStringLiteral("-projection")).join(QLatin1Char(' '));
            }
            QStringList scaleTokens = block.optionsByName.value(QStringLiteral("scale"));
            if (scaleTokens.isEmpty()) {
                scaleTokens = block.optionsByName.value(QStringLiteral("-scale"));
            }
            scaleOption = parseScaleOption(scaleTokens);
            break;
        }
    }

    // Extract station anchors
    for (const auto &point : th2Projection.points()) {
        if (!scrapRange.containsLine(point.command.sourceRange.startLineNumber)) {
            continue;
        }
        if (point.command.type.compare(QStringLiteral("station"), Qt::CaseInsensitive) == 0 && point.hasPosition) {
            QString stationName = point.command.id;
            if (stationName.isEmpty()) {
                const QStringList nameTokens = point.command.optionsByName.value(QStringLiteral("name"));
                if (!nameTokens.isEmpty()) {
                    stationName = nameTokens.first();
                } else {
                    const QStringList dashTokens = point.command.optionsByName.value(QStringLiteral("-name"));
                    if (!dashTokens.isEmpty()) {
                        stationName = dashTokens.first();
                    }
                }
            }
            if (!stationName.isEmpty()) {
                TherionScrapAnchor anchor;
                anchor.stationName = stationName;
                anchor.scrapPoint = point.position;
                anchor.resolvedStationKey = resolveStationKey(anchor.stationName, scrapSurveyPath, stationPositions);
                if (!anchor.resolvedStationKey.isEmpty() && stationPositions.contains(anchor.resolvedStationKey)) {
                    anchor.centrelinePosition = stationPositions.value(anchor.resolvedStationKey);
                    anchor.matched = true;
                }
                model.anchors.append(anchor);
            }
        }
    }

    model.transform = fitTransform(model.anchors, scaleOption);

    // Filter matched anchors for Z-interpolation
    QVector<TherionScrapAnchor> matchedAnchors;
    for (const auto &a : model.anchors) {
        if (a.matched) {
            matchedAnchors.append(a);
        }
    }

    // Compute residuals for each anchor
    for (auto &anchor : model.anchors) {
        if (anchor.matched) {
            const QPointF fitted = model.transform.transformPoint(anchor.scrapPoint);
            anchor.residual = std::hypot(fitted.x() - anchor.centrelinePosition.x,
                                         fitted.y() - anchor.centrelinePosition.y);
        }
    }

    if (!model.transform.valid) {
        model.diagnostics.append(QStringLiteral("No matching centreline station anchor; geometry was omitted."));
        return model;
    }

    // Build the wall surface before projecting other geometry so every feature can use it as its Z source.
    model.meshGroup.id = meshId;
    model.meshGroup.surveyId = surveyId;
    model.meshGroup.colorRgba = 0xD8FFFDE7u;
    model.meshGroup.hasColor = true;

    struct PreparedLine
    {
        const Th2LineObject *line = nullptr;
        QVector<QPointF> points;
    };
    QVector<PreparedLine> preparedLines;
    QVector<QVector<QPointF>> wallLines;
    for (const Th2LineObject &line : th2Projection.lines()) {
        if (!scrapRange.containsLine(line.command.sourceRange.startLineNumber)
            || options.hiddenObjectLines.contains(line.command.sourceRange.startLineNumber)
            || (options.wallsOnly && line.command.type.compare(QStringLiteral("wall"), Qt::CaseInsensitive) != 0)) {
            continue;
        }
        if (options.objectsOnly
            && (line.command.type.compare(QStringLiteral("border"), Qt::CaseInsensitive) == 0
                || (line.command.type.compare(QStringLiteral("wall"), Qt::CaseInsensitive) == 0
                    && line.command.subtype.compare(QStringLiteral("invisible"), Qt::CaseInsensitive) == 0))) {
            continue;
        }
        QVector<QPointF> polyline2D = polyline2DForLine(line);
        if (polyline2D.size() < 2) {
            continue;
        }
        if (line.command.type.startsWith(QLatin1String("wall"), Qt::CaseInsensitive)) {
            wallLines.append(polyline2D);
        }
        preparedLines.append({&line, std::move(polyline2D)});
    }

    ThreeDViewerMeshGroup wallSurface = model.meshGroup;
    const QVector<QVector<QPointF>> wallRings = closedWallRings(wallLines);
    for (const QVector<QPointF> &ring : wallRings) {
        const int previousTriangleCount = wallSurface.triangles.size();
        appendAreaMesh(&wallSurface, ring, model.transform, matchedAnchors);
        if (wallSurface.triangles.size() == previousTriangleCount) {
            model.diagnostics.append(QStringLiteral("Wall boundary was omitted because it could not be triangulated."));
        }
    }
    if (!wallLines.isEmpty() && wallRings.isEmpty()) {
        model.diagnostics.append(QStringLiteral("Wall lines do not form a closed boundary; the sheet surface was omitted."));
    }

    for (const PreparedLine &prepared : preparedLines) {
        QVector<ThreeDViewerVec3> polyline3D;
        polyline3D.reserve(prepared.points.size());
        for (const QPointF &point : prepared.points) {
            const QPointF horizontal = model.transform.transformPoint(point);
            const double fallbackZ = model.transform.interpolateZ(point, matchedAnchors);
            const double z = zOnMeshSurface(wallSurface, horizontal, fallbackZ);
            polyline3D.append({horizontal.x(), horizontal.y(), z});
        }
        model.linework.append(ThreeDViewerScrapPolyline{
            prepared.line->command.type,
            prepared.line->command.subtype,
            polyline3D,
            prepared.line->closed,
        });
        model.linework.last().sourceId = prepared.line->command.id;
        if (prepared.line->command.type.startsWith(QLatin1String("wall"), Qt::CaseInsensitive)) {
            model.wallPolylines.append(polyline3D);
        }
    }

    if (options.wallsOnly) {
        return model;
    }

    for (const Th2PointObject &point : th2Projection.points()) {
        if (!scrapRange.containsLine(point.command.sourceRange.startLineNumber)
            || options.hiddenObjectLines.contains(point.command.sourceRange.startLineNumber)
            || !point.hasPosition
            || point.command.type.compare(QStringLiteral("station"), Qt::CaseInsensitive) == 0) {
            continue;
        }
        const QPointF horizontal = model.transform.transformPoint(point.position);
        const double fallbackZ = model.transform.interpolateZ(point.position, matchedAnchors);
        const double z = zOnMeshSurface(wallSurface, horizontal, fallbackZ);
        model.pointFeatures.append(ThreeDViewerScrapPoint{
            point.command.type,
            point.command.subtype,
            {horizontal.x(), horizontal.y(), z},
        });
        auto &feature = model.pointFeatures.last();
        feature.text = point.command.optionsByName.value(QStringLiteral("text")).join(QLatin1Char(' '));
        feature.orientationDegrees = point.command.optionsByName.value(QStringLiteral("orientation")).value(0).toDouble();
    }
    if (options.objectsOnly) {
        return model;
    }

    model.meshGroup = std::move(wallSurface);
    QHash<QString, const Th2LineObject *> linesById;
    for (const Th2LineObject &line : th2Projection.lines()) {
        if (scrapRange.containsLine(line.command.sourceRange.startLineNumber) && !line.command.id.isEmpty()) {
            linesById.insert(line.command.id, &line);
        }
        if (!scrapRange.containsLine(line.command.sourceRange.startLineNumber)) {
            continue;
        }
    }

    for (const Th2AreaObject &area : th2Projection.areas()) {
        if (!scrapRange.containsLine(area.command.sourceRange.startLineNumber)) {
            continue;
        }
        if (area.borderReferences.size() != 1) {
            model.diagnostics.append(QStringLiteral("Area '%1' was omitted because it does not have exactly one border.")
                                         .arg(area.command.type));
            continue;
        }
        const Th2LineObject *border = linesById.value(area.borderReferences.first(), nullptr);
        if (border == nullptr) {
            model.diagnostics.append(QStringLiteral("Area '%1' was omitted because its border line was not found.")
                                         .arg(area.command.type));
            continue;
        }
        const QVector<QPointF> ring = polyline2DForLine(*border);
        const int previousVertexCount = model.meshGroup.vertices.size();
        appendAreaMesh(&model.meshGroup, ring, model.transform, matchedAnchors);
        if (model.meshGroup.vertices.size() == previousVertexCount) {
            model.diagnostics.append(QStringLiteral("Area '%1' was omitted because its border could not be triangulated.")
                                         .arg(area.command.type));
        }
    }

    model.meshGroup.computeNormals();
    return model;
}

QVector<TherionScrap3DModel> TherionScrap3D::projectTh2File(
    const QString &filePath,
    const QString &th2Contents,
    const QString &surveyPath,
    const QHash<QString, ThreeDViewerVec3> &stationPositions,
    quint32 startMeshId,
    quint32 surveyId,
    const TherionScrap3DOptions &options)
{
    QVector<TherionScrap3DModel> results;
    const Th2GeometryProjection projection = Th2GeometryProjection::fromText(th2Contents);

    quint32 currentMeshId = startMeshId;
    for (const auto &block : projection.blockObjects()) {
        if (block.kind == Th2GeometryObjectKind::Scrap
            && !options.hiddenObjectLines.contains(block.sourceRange.startLineNumber)) {
            const QString scrapId = !block.type.isEmpty() ? block.type : block.id;
            TherionScrap3DModel model = projectScrap(
                projection, scrapId, surveyPath, stationPositions, currentMeshId++, surveyId, options);
            model.filePath = filePath;
            results.append(model);
        }
    }

    return results;
}


QVector<TherionScrapJoinDiagnostic> TherionScrap3D::applyJoins(
    QVector<TherionScrap3DModel> *models,
    const QVector<TherionScrapJoin> &joins)
{
    QVector<TherionScrapJoinDiagnostic> diagnostics;
    if (models == nullptr || models->isEmpty() || joins.isEmpty()) {
        return diagnostics;
    }

    struct ParsedReference
    {
        QString objectId;
        QString surveyPath;
        QString marker;
    };
    const auto parseReference = [](const QString &reference) {
        ParsedReference parsed;
        QString object = reference.trimmed();
        const qsizetype markerIndex = object.indexOf(QLatin1Char(':'));
        if (markerIndex >= 0) {
            parsed.marker = object.mid(markerIndex + 1).trimmed();
            object.truncate(markerIndex);
        }
        const qsizetype namespaceIndex = object.indexOf(QLatin1Char('@'));
        if (namespaceIndex >= 0) {
            parsed.objectId = object.left(namespaceIndex).trimmed();
            parsed.surveyPath = object.mid(namespaceIndex + 1).trimmed();
        } else {
            parsed.objectId = object.trimmed();
        }
        return parsed;
    };
    const auto surveyPathMatches = [](const QString &candidate, const QString &requested) {
        if (requested.isEmpty()) {
            return true;
        }
        return candidate == requested
            || candidate.startsWith(requested + QLatin1Char('.'))
            || candidate.endsWith(QLatin1Char('.') + requested);
    };
    const auto addDiagnostic = [&diagnostics](const TherionScrapJoin &join, const QString &message) {
        diagnostics.append({join.filePath, join.lineNumber, message});
    };

    struct LineLocation
    {
        int modelIndex = -1;
        int lineIndex = -1;
    };
    QVector<QVector<std::array<int, 2>>> endpointNodesByLine;
    QVector<ThreeDViewerVec3> endpointPositions;
    QVector<char> joinedEndpoint;
    for (int modelIndex = 0; modelIndex < models->size(); ++modelIndex) {
        const TherionScrap3DModel &model = models->at(modelIndex);
        QVector<std::array<int, 2>> modelEndpointNodes;
        modelEndpointNodes.reserve(model.linework.size());
        for (int lineIndex = 0; lineIndex < model.linework.size(); ++lineIndex) {
            const ThreeDViewerScrapPolyline &line = model.linework.at(lineIndex);
            if (line.points.size() < 2) {
                modelEndpointNodes.append({-1, -1});
                continue;
            }
            const int firstNode = endpointPositions.size();
            endpointPositions.append(line.points.first());
            joinedEndpoint.append(0);
            const int lastNode = endpointPositions.size();
            endpointPositions.append(line.points.last());
            joinedEndpoint.append(0);
            modelEndpointNodes.append({firstNode, lastNode});
        }
        endpointNodesByLine.append(modelEndpointNodes);
    }

    QVector<int> parents;
    parents.reserve(endpointPositions.size());
    for (int node = 0; node < endpointPositions.size(); ++node) {
        parents.append(node);
    }
    const auto rootOf = [&parents](int node) {
        int root = node;
        while (parents.at(root) != root) {
            root = parents.at(root);
        }
        while (parents.at(node) != node) {
            const int next = parents.at(node);
            parents[node] = root;
            node = next;
        }
        return root;
    };
    const auto joinNodes = [&parents, &joinedEndpoint, &rootOf](int left, int right) {
        if (left < 0 || right < 0) {
            return;
        }
        const int leftRoot = rootOf(left);
        const int rightRoot = rootOf(right);
        if (leftRoot != rightRoot) {
            parents[rightRoot] = leftRoot;
        }
        joinedEndpoint[left] = 1;
        joinedEndpoint[right] = 1;
    };

    const auto matchingModels = [&models, &surveyPathMatches](const ParsedReference &reference,
                                                              const TherionScrapJoin &join) {
        QVector<int> matches;
        const QString requestedPath = reference.surveyPath.isEmpty() ? join.surveyPath : reference.surveyPath;
        for (int index = 0; index < models->size(); ++index) {
            const TherionScrap3DModel &model = models->at(index);
            if (model.scrapId == reference.objectId && surveyPathMatches(model.surveyPath, requestedPath)) {
                matches.append(index);
            }
        }
        return matches;
    };
    const auto matchingLines = [&models, &surveyPathMatches](const ParsedReference &reference,
                                                              const TherionScrapJoin &join) {
        QVector<LineLocation> matches;
        const QString requestedPath = reference.surveyPath.isEmpty() ? join.surveyPath : reference.surveyPath;
        for (int modelIndex = 0; modelIndex < models->size(); ++modelIndex) {
            const TherionScrap3DModel &model = models->at(modelIndex);
            if (!surveyPathMatches(model.surveyPath, requestedPath)) {
                continue;
            }
            for (int lineIndex = 0; lineIndex < model.linework.size(); ++lineIndex) {
                if (model.linework.at(lineIndex).sourceId == reference.objectId) {
                    matches.append({modelIndex, lineIndex});
                }
            }
        }
        return matches;
    };
    const auto endpointForMarker = [](const QString &marker, int firstNode, int lastNode) {
        if (marker.compare(QStringLiteral("0"), Qt::CaseInsensitive) == 0) {
            return firstNode;
        }
        if (marker.compare(QStringLiteral("end"), Qt::CaseInsensitive) == 0) {
            return lastNode;
        }
        return -1;
    };

    struct EndpointPair
    {
        int firstNode = -1;
        int secondNode = -1;
        double distance = 0.0;
    };
    for (const TherionScrapJoin &join : joins) {
        const ParsedReference first = parseReference(join.firstReference);
        const ParsedReference second = parseReference(join.secondReference);
        if (first.objectId.isEmpty() || second.objectId.isEmpty()) {
            addDiagnostic(join, QStringLiteral("A join reference is empty or malformed."));
            continue;
        }

        if (!first.marker.isEmpty() || !second.marker.isEmpty()) {
            const QVector<LineLocation> firstLines = matchingLines(first, join);
            const QVector<LineLocation> secondLines = matchingLines(second, join);
            if (firstLines.size() != 1 || secondLines.size() != 1) {
                addDiagnostic(join, QStringLiteral("A joined line reference could not be resolved uniquely."));
                continue;
            }
            const LineLocation firstLine = firstLines.first();
            const LineLocation secondLine = secondLines.first();
            const auto firstNodes = endpointNodesByLine.at(firstLine.modelIndex).at(firstLine.lineIndex);
            const auto secondNodes = endpointNodesByLine.at(secondLine.modelIndex).at(secondLine.lineIndex);
            const int firstNode = endpointForMarker(first.marker, firstNodes[0], firstNodes[1]);
            const int secondNode = endpointForMarker(second.marker, secondNodes[0], secondNodes[1]);
            if (firstNode < 0 || secondNode < 0) {
                addDiagnostic(join, QStringLiteral("Only line point markers '0' and 'end' are supported for 3D joins."));
                continue;
            }
            joinNodes(firstNode, secondNode);
            continue;
        }

        const QVector<int> firstModels = matchingModels(first, join);
        const QVector<int> secondModels = matchingModels(second, join);
        if (firstModels.size() != 1 || secondModels.size() != 1) {
            addDiagnostic(join, QStringLiteral("A joined scrap reference could not be resolved uniquely."));
            continue;
        }

        QVector<EndpointPair> candidates;
        for (const int firstModelIndex : firstModels) {
            const TherionScrap3DModel &firstModel = models->at(firstModelIndex);
            for (int firstLineIndex = 0; firstLineIndex < firstModel.linework.size(); ++firstLineIndex) {
                const ThreeDViewerScrapPolyline &firstLine = firstModel.linework.at(firstLineIndex);
                if (!firstLine.type.startsWith(QLatin1String("wall"), Qt::CaseInsensitive)
                    || firstLine.points.size() < 2) {
                    continue;
                }
                const auto firstNodes = endpointNodesByLine.at(firstModelIndex).at(firstLineIndex);
                for (const int secondModelIndex : secondModels) {
                    const TherionScrap3DModel &secondModel = models->at(secondModelIndex);
                    for (int secondLineIndex = 0; secondLineIndex < secondModel.linework.size(); ++secondLineIndex) {
                        const ThreeDViewerScrapPolyline &secondLine = secondModel.linework.at(secondLineIndex);
                        if (!secondLine.type.startsWith(QLatin1String("wall"), Qt::CaseInsensitive)
                            || secondLine.points.size() < 2) {
                            continue;
                        }
                        const auto secondNodes = endpointNodesByLine.at(secondModelIndex).at(secondLineIndex);
                        for (int firstSide = 0; firstSide < 2; ++firstSide) {
                            for (int secondSide = 0; secondSide < 2; ++secondSide) {
                                const ThreeDViewerVec3 &left = endpointPositions.at(firstNodes[firstSide]);
                                const ThreeDViewerVec3 &right = endpointPositions.at(secondNodes[secondSide]);
                                const double dx = left.x - right.x;
                                const double dy = left.y - right.y;
                                const double dz = left.z - right.z;
                                candidates.append({firstNodes[firstSide], secondNodes[secondSide],
                                                   std::sqrt(dx * dx + dy * dy + dz * dz)});
                            }
                        }
                    }
                }
            }
        }
        std::sort(candidates.begin(), candidates.end(), [](const EndpointPair &left, const EndpointPair &right) {
            return left.distance < right.distance;
        });
        QSet<int> usedFirstEndpoints;
        QSet<int> usedSecondEndpoints;
        int matchedCount = 0;
        for (const EndpointPair &candidate : candidates) {
            if (usedFirstEndpoints.contains(candidate.firstNode)
                || usedSecondEndpoints.contains(candidate.secondNode)) {
                continue;
            }
            joinNodes(candidate.firstNode, candidate.secondNode);
            usedFirstEndpoints.insert(candidate.firstNode);
            usedSecondEndpoints.insert(candidate.secondNode);
            if (++matchedCount >= join.count) {
                break;
            }
        }
        if (matchedCount == 0 || matchedCount < join.count) {
            addDiagnostic(join, QStringLiteral("The scrap join matched %1 of %2 requested wall connections.")
                                     .arg(matchedCount).arg(join.count));
        }
    }

    QHash<int, ThreeDViewerVec3> joinedPositionSums;
    QHash<int, int> joinedPositionCounts;
    for (int node = 0; node < endpointPositions.size(); ++node) {
        if (!joinedEndpoint.at(node)) {
            continue;
        }
        const int root = rootOf(node);
        ThreeDViewerVec3 &sum = joinedPositionSums[root];
        const ThreeDViewerVec3 &position = endpointPositions.at(node);
        sum.x += position.x;
        sum.y += position.y;
        sum.z += position.z;
        ++joinedPositionCounts[root];
    }
    QHash<int, ThreeDViewerVec3> joinedPositions;
    for (auto it = joinedPositionSums.cbegin(); it != joinedPositionSums.cend(); ++it) {
        const int count = joinedPositionCounts.value(it.key());
        joinedPositions.insert(it.key(), {it->x / count, it->y / count, it->z / count});
    }

    for (int modelIndex = 0; modelIndex < models->size(); ++modelIndex) {
        TherionScrap3DModel &model = (*models)[modelIndex];
        for (int lineIndex = 0; lineIndex < model.linework.size(); ++lineIndex) {
            ThreeDViewerScrapPolyline &line = model.linework[lineIndex];
            if (line.points.size() < 2) {
                continue;
            }
            const auto nodes = endpointNodesByLine.at(modelIndex).at(lineIndex);
            const bool moveStart = nodes[0] >= 0 && joinedEndpoint.at(nodes[0]);
            const bool moveEnd = nodes[1] >= 0 && joinedEndpoint.at(nodes[1]);
            if (!moveStart && !moveEnd) {
                continue;
            }
            const ThreeDViewerVec3 startTarget = moveStart ? joinedPositions.value(rootOf(nodes[0])) : line.points.first();
            const ThreeDViewerVec3 endTarget = moveEnd ? joinedPositions.value(rootOf(nodes[1])) : line.points.last();
            const ThreeDViewerVec3 startDelta{startTarget.x - line.points.first().x,
                                              startTarget.y - line.points.first().y,
                                              startTarget.z - line.points.first().z};
            const ThreeDViewerVec3 endDelta{endTarget.x - line.points.last().x,
                                            endTarget.y - line.points.last().y,
                                            endTarget.z - line.points.last().z};
            double totalLength = 0.0;
            QVector<double> distances{0.0};
            for (int pointIndex = 1; pointIndex < line.points.size(); ++pointIndex) {
                const ThreeDViewerVec3 &previous = line.points.at(pointIndex - 1);
                const ThreeDViewerVec3 &current = line.points.at(pointIndex);
                totalLength += std::hypot(current.x - previous.x,
                                           current.y - previous.y,
                                           current.z - previous.z);
                distances.append(totalLength);
            }
            for (int pointIndex = 0; pointIndex < line.points.size(); ++pointIndex) {
                const double t = totalLength > 1e-9
                    ? distances.at(pointIndex) / totalLength
                    : double(pointIndex) / double(line.points.size() - 1);
                const double startWeight = moveStart ? 1.0 - t : 0.0;
                const double endWeight = moveEnd ? t : 0.0;
                ThreeDViewerVec3 &point = line.points[pointIndex];
                point.x += startWeight * startDelta.x + endWeight * endDelta.x;
                point.y += startWeight * startDelta.y + endWeight * endDelta.y;
                point.z += startWeight * startDelta.z + endWeight * endDelta.z;
            }
        }
        model.wallPolylines.clear();
        for (const ThreeDViewerScrapPolyline &line : model.linework) {
            if (line.type.startsWith(QLatin1String("wall"), Qt::CaseInsensitive)) {
                model.wallPolylines.append(line.points);
            }
        }
    }

    struct JoinedWallLine
    {
        int modelIndex = -1;
        int startNode = -1;
        int endNode = -1;
        QVector<ThreeDViewerVec3> points;
    };
    QVector<JoinedWallLine> wallLines;
    for (int modelIndex = 0; modelIndex < models->size(); ++modelIndex) {
        const TherionScrap3DModel &model = models->at(modelIndex);
        for (int lineIndex = 0; lineIndex < model.linework.size(); ++lineIndex) {
            const ThreeDViewerScrapPolyline &line = model.linework.at(lineIndex);
            if (line.type.startsWith(QLatin1String("wall"), Qt::CaseInsensitive) && line.points.size() >= 2) {
                const auto nodes = endpointNodesByLine.at(modelIndex).at(lineIndex);
                wallLines.append({modelIndex, nodes[0], nodes[1], line.points});
            }
        }
    }
    struct JoinedRing
    {
        QVector<ThreeDViewerVec3> points;
        QSet<int> modelIndexes;
    };
    QVector<JoinedRing> joinedRings;
    QVector<bool> consumed(wallLines.size(), false);
    const auto sameHorizontalPoint = [](const ThreeDViewerVec3 &left, const ThreeDViewerVec3 &right) {
        return std::hypot(left.x - right.x, left.y - right.y) <= 1e-6;
    };
    const auto endpointsConnected = [&joinedEndpoint, &rootOf](int leftNode, int leftModel,
                                                                int rightNode, int rightModel) {
        if (leftModel == rightModel) {
            return true;
        }
        return leftNode >= 0 && rightNode >= 0 && joinedEndpoint.at(leftNode) && joinedEndpoint.at(rightNode)
            && rootOf(leftNode) == rootOf(rightNode);
    };
    for (int startIndex = 0; startIndex < wallLines.size(); ++startIndex) {
        if (consumed.at(startIndex)) {
            continue;
        }
        const JoinedWallLine &startLine = wallLines.at(startIndex);
        QVector<ThreeDViewerVec3> chain = startLine.points;
        QSet<int> ringModels{startLine.modelIndex};
        consumed[startIndex] = true;
        int chainStartNode = startLine.startNode;
        int chainEndNode = startLine.endNode;
        int chainStartModel = startLine.modelIndex;
        int chainEndModel = startLine.modelIndex;
        if (sameHorizontalPoint(chain.first(), chain.last())
            && endpointsConnected(chainEndNode, chainEndModel, chainStartNode, chainStartModel)) {
            joinedRings.append({chain, ringModels});
            continue;
        }
        while (true) {
            if (sameHorizontalPoint(chain.last(), chain.first())
                && endpointsConnected(chainEndNode, chainEndModel, chainStartNode, chainStartModel)) {
                joinedRings.append({chain, ringModels});
                break;
            }
            int matchIndex = -1;
            bool reverseMatch = false;
            bool ambiguousMatch = false;
            for (int candidateIndex = 0; candidateIndex < wallLines.size(); ++candidateIndex) {
                if (consumed.at(candidateIndex)) {
                    continue;
                }
                const JoinedWallLine &candidate = wallLines.at(candidateIndex);
                const bool matchesFront = sameHorizontalPoint(chain.last(), candidate.points.first())
                    && endpointsConnected(chainEndNode, chainEndModel, candidate.startNode, candidate.modelIndex);
                const bool matchesBack = sameHorizontalPoint(chain.last(), candidate.points.last())
                    && endpointsConnected(chainEndNode, chainEndModel, candidate.endNode, candidate.modelIndex);
                if (matchesFront == matchesBack) {
                    if (matchesFront) {
                        ambiguousMatch = true;
                        break;
                    }
                    continue;
                }
                if (matchIndex >= 0) {
                    ambiguousMatch = true;
                    break;
                }
                matchIndex = candidateIndex;
                reverseMatch = matchesBack;
            }
            if (ambiguousMatch || matchIndex < 0) {
                break;
            }
            consumed[matchIndex] = true;
            const JoinedWallLine &nextLine = wallLines.at(matchIndex);
            if (reverseMatch) {
                for (int pointIndex = nextLine.points.size() - 2; pointIndex >= 0; --pointIndex) {
                    chain.append(nextLine.points.at(pointIndex));
                }
                chainEndNode = nextLine.startNode;
            } else {
                for (int pointIndex = 1; pointIndex < nextLine.points.size(); ++pointIndex) {
                    chain.append(nextLine.points.at(pointIndex));
                }
                chainEndNode = nextLine.endNode;
            }
            chainEndModel = nextLine.modelIndex;
            ringModels.insert(nextLine.modelIndex);
        }
    }

    ThreeDViewerMeshGroup joinedSurface;
    for (const JoinedRing &ring : joinedRings) {
        if (ring.modelIndexes.size() < 2) {
            continue;
        }
        QVector<QPointF> polygon;
        polygon.reserve(ring.points.size());
        for (const ThreeDViewerVec3 &point : ring.points) {
            polygon.append(QPointF(point.x, point.y));
        }
        if (polygon.size() > 1 && QLineF(polygon.first(), polygon.last()).length() <= 1e-6) {
            polygon.removeLast();
        }
        const QVector<std::array<int, 3>> triangles = triangulateSimpleRing(polygon);
        if (triangles.isEmpty()) {
            continue;
        }
        int ownerModel = -1;
        for (const int modelIndex : ring.modelIndexes) {
            ownerModel = ownerModel < 0 ? modelIndex : std::min(ownerModel, modelIndex);
        }
        ThreeDViewerMeshGroup &targetMesh = (*models)[ownerModel].meshGroup;
        const quint32 targetFirstVertex = quint32(targetMesh.vertices.size());
        const quint32 joinedFirstVertex = quint32(joinedSurface.vertices.size());
        for (qsizetype pointIndex = 0; pointIndex < polygon.size(); ++pointIndex) {
            const ThreeDViewerVec3 &point = ring.points.at(pointIndex);
            targetMesh.vertices.append(point);
            joinedSurface.vertices.append(point);
        }
        for (const auto &triangle : triangles) {
            targetMesh.triangles.append({targetFirstVertex + quint32(triangle[0]),
                                         targetFirstVertex + quint32(triangle[1]),
                                         targetFirstVertex + quint32(triangle[2])});
            joinedSurface.triangles.append({joinedFirstVertex + quint32(triangle[0]),
                                            joinedFirstVertex + quint32(triangle[1]),
                                            joinedFirstVertex + quint32(triangle[2])});
        }
    }
    for (TherionScrap3DModel &model : *models) {
        if (!model.meshGroup.triangles.isEmpty()) {
            model.meshGroup.computeNormals();
        }
        for (ThreeDViewerScrapPolyline &line : model.linework) {
            for (ThreeDViewerVec3 &point : line.points) {
                point.z = zOnMeshSurface(joinedSurface, QPointF(point.x, point.y), point.z);
            }
        }
        for (ThreeDViewerScrapPoint &point : model.pointFeatures) {
            point.position.z = zOnMeshSurface(joinedSurface, QPointF(point.position.x, point.position.y), point.position.z);
        }
        model.wallPolylines.clear();
        for (const ThreeDViewerScrapPolyline &line : model.linework) {
            if (line.type.startsWith(QLatin1String("wall"), Qt::CaseInsensitive)) {
                model.wallPolylines.append(line.points);
            }
        }
    }
    return diagnostics;
}

} // namespace TherionStudio
