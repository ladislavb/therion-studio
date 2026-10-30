#include "TherionCentreline.h"

#include <QSet>

#include <algorithm>
#include <cmath>
#include <numeric>

namespace TherionStudio
{
namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kMinimumShotWeightLength = 0.01;

struct DisjointSet
{
    QVector<int> parent;

    int add()
    {
        parent.append(parent.size());
        return parent.size() - 1;
    }

    int find(int node)
    {
        while (parent[node] != node) {
            parent[node] = parent[parent[node]];
            node = parent[node];
        }
        return node;
    }

    void unite(int left, int right)
    {
        left = find(left);
        right = find(right);
        if (left != right) {
            parent[right] = left;
        }
    }
};

struct Edge
{
    int from = 0;
    int to = 0;
    ThreeDViewerVec3 delta;
    double weight = 1.0;
    int shotIndex = -1;
};

struct NeighbourEntry
{
    int node = 0;
    double weight = 0.0;
};

double component(const ThreeDViewerVec3 &vector, int axis)
{
    return axis == 0 ? vector.x : (axis == 1 ? vector.y : vector.z);
}

void setComponent(ThreeDViewerVec3 *vector, int axis, double value)
{
    (axis == 0 ? vector->x : (axis == 1 ? vector->y : vector->z)) = value;
}

// Weighted least squares over the shot network: minimise
// sum w * |p_to - p_from - delta|^2 with constrained nodes held fixed.
// Returns false for nodes that could not be positioned.
QVector<bool> solveNetwork(int nodeCount,
                           const QVector<Edge> &edges,
                           const QHash<int, ThreeDViewerVec3> &constraints,
                           QVector<ThreeDViewerVec3> *positions)
{
    positions->fill(ThreeDViewerVec3(), nodeCount);
    QVector<bool> positioned(nodeCount, false);
    QVector<QVector<int>> incident(nodeCount);
    for (int index = 0; index < edges.size(); ++index) {
        incident[edges[index].from].append(index);
        incident[edges[index].to].append(index);
    }

    // Initial guess: propagate along shots from the constrained nodes. This is
    // exact for loop-free networks, so the solver only has to fix loops.
    QVector<int> queue;
    for (auto it = constraints.constBegin(); it != constraints.constEnd(); ++it) {
        (*positions)[it.key()] = it.value();
        positioned[it.key()] = true;
        queue.append(it.key());
    }
    for (int head = 0; head < queue.size(); ++head) {
        const int node = queue[head];
        for (int edgeIndex : std::as_const(incident[node])) {
            const Edge &edge = edges[edgeIndex];
            const int other = edge.from == node ? edge.to : edge.from;
            if (positioned[other]) {
                continue;
            }
            const double sign = edge.from == node ? 1.0 : -1.0;
            const ThreeDViewerVec3 &base = (*positions)[node];
            (*positions)[other] = {base.x + sign * edge.delta.x, base.y + sign * edge.delta.y,
                                   base.z + sign * edge.delta.z};
            positioned[other] = true;
            queue.append(other);
        }
    }

    QVector<int> unknownIndex(nodeCount, -1);
    QVector<int> unknownNodes;
    for (int node = 0; node < nodeCount; ++node) {
        if (positioned[node] && !constraints.contains(node)) {
            unknownIndex[node] = unknownNodes.size();
            unknownNodes.append(node);
        }
    }
    const int unknownCount = unknownNodes.size();
    if (unknownCount == 0) {
        return positioned;
    }

    QVector<double> diagonal(unknownCount, 0.0);
    QVector<QVector<NeighbourEntry>> neighbours(unknownCount);
    for (const Edge &edge : edges) {
        const int from = unknownIndex[edge.from];
        const int to = unknownIndex[edge.to];
        if (from >= 0) {
            diagonal[from] += edge.weight;
        }
        if (to >= 0) {
            diagonal[to] += edge.weight;
        }
        if (from >= 0 && to >= 0) {
            neighbours[from].append({to, edge.weight});
            neighbours[to].append({from, edge.weight});
        }
    }

    auto multiply = [&](const QVector<double> &vector, QVector<double> *result) {
        for (int row = 0; row < unknownCount; ++row) {
            double value = diagonal[row] * vector[row];
            for (const NeighbourEntry &entry : std::as_const(neighbours[row])) {
                value -= entry.weight * vector[entry.node];
            }
            (*result)[row] = value;
        }
    };

    for (int axis = 0; axis < 3; ++axis) {
        QVector<double> rhs(unknownCount, 0.0);
        for (const Edge &edge : edges) {
            const double weighted = edge.weight * component(edge.delta, axis);
            const int from = unknownIndex[edge.from];
            const int to = unknownIndex[edge.to];
            if (from >= 0) {
                rhs[from] -= weighted;
                if (to < 0) {
                    rhs[from] += edge.weight * component((*positions)[edge.to], axis);
                }
            }
            if (to >= 0) {
                rhs[to] += weighted;
                if (from < 0) {
                    rhs[to] += edge.weight * component((*positions)[edge.from], axis);
                }
            }
        }

        QVector<double> solution(unknownCount);
        for (int row = 0; row < unknownCount; ++row) {
            solution[row] = component((*positions)[unknownNodes[row]], axis);
        }

        // Jacobi-preconditioned conjugate gradients.
        QVector<double> residual(unknownCount);
        QVector<double> product(unknownCount);
        multiply(solution, &product);
        double rhsNorm = 0.0;
        for (int row = 0; row < unknownCount; ++row) {
            residual[row] = rhs[row] - product[row];
            rhsNorm += rhs[row] * rhs[row];
        }
        const double tolerance = 1e-24 * std::max(rhsNorm, 1.0);
        QVector<double> preconditioned(unknownCount);
        for (int row = 0; row < unknownCount; ++row) {
            preconditioned[row] = residual[row] / diagonal[row];
        }
        QVector<double> direction = preconditioned;
        double rho = std::inner_product(residual.cbegin(), residual.cend(), preconditioned.cbegin(), 0.0);
        const int maxIterations = 10 * unknownCount + 100;
        for (int iteration = 0; iteration < maxIterations; ++iteration) {
            const double residualNorm =
                std::inner_product(residual.cbegin(), residual.cend(), residual.cbegin(), 0.0);
            if (residualNorm <= tolerance) {
                break;
            }
            multiply(direction, &product);
            const double curvature =
                std::inner_product(direction.cbegin(), direction.cend(), product.cbegin(), 0.0);
            if (curvature <= 0.0) {
                break;
            }
            const double alpha = rho / curvature;
            for (int row = 0; row < unknownCount; ++row) {
                solution[row] += alpha * direction[row];
                residual[row] -= alpha * product[row];
                preconditioned[row] = residual[row] / diagonal[row];
            }
            const double nextRho =
                std::inner_product(residual.cbegin(), residual.cend(), preconditioned.cbegin(), 0.0);
            const double beta = nextRho / rho;
            rho = nextRho;
            for (int row = 0; row < unknownCount; ++row) {
                direction[row] = preconditioned[row] + beta * direction[row];
            }
        }

        for (int row = 0; row < unknownCount; ++row) {
            setComponent(&(*positions)[unknownNodes[row]], axis, solution[row]);
        }
    }
    return positioned;
}

struct FixFrame
{
    double rotationRadians = 0.0;
    bool mirrored = false;
    double residual = 0.0;
    bool estimated = false;

    ThreeDViewerVec3 apply(const ThreeDViewerVec3 &vector) const
    {
        const double x = mirrored ? -vector.x : vector.x;
        const double c = std::cos(rotationRadians);
        const double s = std::sin(rotationRadians);
        return {x * c - vector.y * s, x * s + vector.y * c, vector.z};
    }
};

// Horizontal rigid fit (optionally mirrored) of the free network onto the fixes.
FixFrame estimateFixFrame(const QVector<QVector<int>> &fixedNodesByComponent,
                          const QVector<ThreeDViewerVec3> &freePositions,
                          const QHash<int, ThreeDViewerVec3> &fixPositions)
{
    double dot = 0.0;
    double cross = 0.0;
    double mirroredDot = 0.0;
    double mirroredCross = 0.0;
    double freeSpread = 0.0;
    double fixSpread = 0.0;
    int count = 0;
    for (const QVector<int> &nodes : fixedNodesByComponent) {
        if (nodes.size() < 2) {
            continue;
        }
        ThreeDViewerVec3 freeCentre;
        ThreeDViewerVec3 fixCentre;
        for (int node : nodes) {
            freeCentre.x += freePositions[node].x / nodes.size();
            freeCentre.y += freePositions[node].y / nodes.size();
            fixCentre.x += fixPositions[node].x / nodes.size();
            fixCentre.y += fixPositions[node].y / nodes.size();
        }
        for (int node : nodes) {
            const double ax = freePositions[node].x - freeCentre.x;
            const double ay = freePositions[node].y - freeCentre.y;
            const double bx = fixPositions[node].x - fixCentre.x;
            const double by = fixPositions[node].y - fixCentre.y;
            dot += ax * bx + ay * by;
            cross += ax * by - ay * bx;
            mirroredDot += -ax * bx + ay * by;
            mirroredCross += -ax * by - ay * bx;
            freeSpread += ax * ax + ay * ay;
            fixSpread += bx * bx + by * by;
            ++count;
        }
    }

    FixFrame frame;
    if (count < 2 || freeSpread < 1.0) {
        return frame;
    }
    const double properFit = std::hypot(dot, cross);
    const double mirroredFit = std::hypot(mirroredDot, mirroredCross);
    const double properResidual = std::max(0.0, freeSpread + fixSpread - 2.0 * properFit);
    const double mirroredResidual = std::max(0.0, freeSpread + fixSpread - 2.0 * mirroredFit);
    // Two fixes cannot distinguish a mirror; require clear evidence for it.
    frame.mirrored = count >= 3 && mirroredResidual * 4.0 < properResidual;
    frame.rotationRadians = frame.mirrored ? std::atan2(mirroredCross, mirroredDot) : std::atan2(cross, dot);
    frame.residual = std::sqrt((frame.mirrored ? mirroredResidual : properResidual) / count);
    frame.estimated = true;
    return frame;
}

QString stationNamePart(const QString &key)
{
    const int at = key.indexOf(QLatin1Char('@'));
    return at >= 0 ? key.left(at) : key;
}

QString stationSurveyPart(const QString &key)
{
    const int at = key.indexOf(QLatin1Char('@'));
    return at >= 0 ? key.mid(at + 1) : QString();
}
} // namespace

TherionCentrelineResult solveTherionCentreline(const TherionCentrelineObservations &observations)
{
    TherionCentrelineResult result;
    result.diagnostics = observations.diagnostics;

    QHash<QString, int> keyIndex;
    QStringList keys;
    DisjointSet equated;
    auto indexOf = [&](const QString &key) {
        const auto it = keyIndex.constFind(key);
        if (it != keyIndex.constEnd()) {
            return it.value();
        }
        const int index = equated.add();
        keyIndex.insert(key, index);
        keys.append(key);
        return index;
    };
    for (const TherionCentrelineShot &shot : observations.shots) {
        indexOf(shot.fromKey);
        if (!shot.toKey.isEmpty()) {
            indexOf(shot.toKey);
        }
    }
    for (const TherionCentrelineFix &fix : observations.fixes) {
        indexOf(fix.stationKey);
    }
    for (const QStringList &equate : observations.equates) {
        const int first = indexOf(equate.first());
        for (int index = 1; index < equate.size(); ++index) {
            equated.unite(first, indexOf(equate.at(index)));
        }
    }

    // Collapse equated station keys into network nodes.
    QHash<int, int> nodeForRoot;
    QVector<int> nodeForKey(keys.size());
    for (int keyIndexValue = 0; keyIndexValue < keys.size(); ++keyIndexValue) {
        const int root = equated.find(keyIndexValue);
        if (!nodeForRoot.contains(root)) {
            nodeForRoot.insert(root, nodeForRoot.size());
        }
        nodeForKey[keyIndexValue] = nodeForRoot.value(root);
    }
    const int nodeCount = nodeForRoot.size();

    QVector<Edge> edges;
    DisjointSet connected;
    for (int node = 0; node < nodeCount; ++node) {
        connected.add();
    }
    for (int shotIndex = 0; shotIndex < observations.shots.size(); ++shotIndex) {
        const TherionCentrelineShot &shot = observations.shots[shotIndex];
        result.statistics.totalLength += shot.splay ? 0.0 : shot.length;
        if (shot.splay || shot.toKey.isEmpty()) {
            ++result.statistics.splayCount;
            continue;
        }
        Edge edge;
        edge.from = nodeForKey[keyIndex.value(shot.fromKey)];
        edge.to = nodeForKey[keyIndex.value(shot.toKey)];
        if (edge.from == edge.to) {
            continue;
        }
        edge.delta = shot.delta;
        edge.weight = 1.0 / std::max(shot.length, kMinimumShotWeightLength);
        edge.shotIndex = shotIndex;
        edges.append(edge);
        connected.unite(edge.from, edge.to);
    }
    result.statistics.shotCount = edges.size();

    QHash<int, ThreeDViewerVec3> fixPositions;
    for (const TherionCentrelineFix &fix : observations.fixes) {
        const int node = nodeForKey[keyIndex.value(fix.stationKey)];
        if (fixPositions.contains(node)) {
            const ThreeDViewerVec3 existing = fixPositions.value(node);
            if (std::hypot(existing.x - fix.position.x, existing.y - fix.position.y, existing.z - fix.position.z)
                > 1e-6) {
                result.diagnostics.append(
                    {fix.filePath, fix.lineNumber,
                     QStringLiteral("Station %1 is fixed more than once; the first fix is used.").arg(fix.stationKey)});
            }
            continue;
        }
        fixPositions.insert(node, fix.position);
    }

    // Components and their fixed nodes.
    QHash<int, int> componentForRoot;
    QVector<int> componentOfNode(nodeCount, -1);
    QVector<bool> nodeUsed(nodeCount, false);
    for (const Edge &edge : std::as_const(edges)) {
        nodeUsed[edge.from] = true;
        nodeUsed[edge.to] = true;
    }
    for (auto it = fixPositions.constBegin(); it != fixPositions.constEnd(); ++it) {
        nodeUsed[it.key()] = true;
    }
    QVector<QVector<int>> fixedNodesByComponent;
    QVector<int> anchorOfComponent;
    for (int node = 0; node < nodeCount; ++node) {
        if (!nodeUsed[node]) {
            continue;
        }
        const int root = connected.find(node);
        if (!componentForRoot.contains(root)) {
            componentForRoot.insert(root, fixedNodesByComponent.size());
            fixedNodesByComponent.append(QVector<int>());
            anchorOfComponent.append(node);
        }
        const int componentIndex = componentForRoot.value(root);
        componentOfNode[node] = componentIndex;
        if (fixPositions.contains(node)) {
            fixedNodesByComponent[componentIndex].append(node);
        }
    }
    result.statistics.componentCount = fixedNodesByComponent.size();

    // Pass 1: free network (true north), one anchor per component.
    QHash<int, ThreeDViewerVec3> freeConstraints;
    for (int componentIndex = 0; componentIndex < fixedNodesByComponent.size(); ++componentIndex) {
        const QVector<int> &fixed = fixedNodesByComponent[componentIndex];
        freeConstraints.insert(fixed.isEmpty() ? anchorOfComponent[componentIndex] : fixed.first(), {});
    }
    QVector<ThreeDViewerVec3> freePositions;
    solveNetwork(nodeCount, edges, freeConstraints, &freePositions);

    const FixFrame frame = estimateFixFrame(fixedNodesByComponent, freePositions, fixPositions);
    result.statistics.fixFrameRotationDegrees = frame.rotationRadians * 180.0 / kPi;
    result.statistics.fixFrameMirrored = frame.mirrored;
    result.statistics.fixFrameResidual = frame.residual;
    if (frame.estimated && frame.residual > 1.0) {
        result.diagnostics.append(
            {QString(), 0,
             QStringLiteral("Fixed stations disagree with the survey shots (%1 m RMS after best-fit rotation).")
                 .arg(frame.residual, 0, 'f', 2)});
    }

    // Pass 2: shots in the fix frame, all fixes held.
    QVector<Edge> frameEdges = edges;
    for (Edge &edge : frameEdges) {
        edge.delta = frame.apply(edge.delta);
    }
    QHash<int, ThreeDViewerVec3> constraints = fixPositions;
    for (int componentIndex = 0; componentIndex < fixedNodesByComponent.size(); ++componentIndex) {
        if (fixedNodesByComponent[componentIndex].isEmpty()) {
            constraints.insert(anchorOfComponent[componentIndex], {});
            ++result.statistics.componentsWithoutFix;
        }
    }
    if (result.statistics.componentCount > 1 && result.statistics.componentsWithoutFix > 0) {
        result.diagnostics.append(
            {QString(), 0,
             QStringLiteral("%1 of %2 disconnected survey parts have no fixed station and are placed at the origin.")
                 .arg(result.statistics.componentsWithoutFix)
                 .arg(result.statistics.componentCount)});
    }
    QVector<ThreeDViewerVec3> positions;
    const QVector<bool> positioned = solveNetwork(nodeCount, frameEdges, constraints, &positions);

    double adjustmentSum = 0.0;
    for (const Edge &edge : std::as_const(frameEdges)) {
        const ThreeDViewerVec3 &from = positions[edge.from];
        const ThreeDViewerVec3 &to = positions[edge.to];
        const double adjustment = std::hypot(to.x - from.x - edge.delta.x, to.y - from.y - edge.delta.y,
                                             to.z - from.z - edge.delta.z);
        adjustmentSum += adjustment;
        result.statistics.maxAdjustment = std::max(result.statistics.maxAdjustment, adjustment);
    }
    result.statistics.meanAdjustment = frameEdges.isEmpty() ? 0.0 : adjustmentSum / frameEdges.size();
    int usedNodes = 0;
    for (bool used : std::as_const(nodeUsed)) {
        usedNodes += used ? 1 : 0;
    }
    result.statistics.loopCount = std::max(0, int(edges.size()) - usedNodes + int(fixedNodesByComponent.size()));

    // Scene model.
    QHash<QString, quint32> surveyIds;
    std::function<quint32(const QString &)> surveyIdFor = [&](const QString &path) -> quint32 {
        if (path.isEmpty()) {
            return 0;
        }
        const auto it = surveyIds.constFind(path);
        if (it != surveyIds.constEnd()) {
            return it.value();
        }
        const int dot = path.indexOf(QLatin1Char('.'));
        const quint32 parentId = dot >= 0 ? surveyIdFor(path.mid(dot + 1)) : 0;
        ThreeDViewerSurvey survey;
        survey.id = quint32(result.scene.surveys.size() + 1);
        survey.parentId = parentId;
        survey.name = dot >= 0 ? path.left(dot) : path;
        result.scene.surveys.append(survey);
        surveyIds.insert(path, survey.id);
        return survey.id;
    };
    for (const QString &path : observations.surveyPaths) {
        surveyIdFor(path);
    }

    QHash<QString, quint32> stationIds;
    for (int keyIndexValue = 0; keyIndexValue < keys.size(); ++keyIndexValue) {
        const int node = nodeForKey[keyIndexValue];
        if (!positioned[node]) {
            continue;
        }
        const QString &key = keys[keyIndexValue];
        ThreeDViewerStation station;
        station.id = quint32(result.scene.stations.size() + 1);
        station.surveyId = surveyIdFor(stationSurveyPart(key));
        station.name = stationNamePart(key);
        station.position = positions[node];
        station.fixed = fixPositions.contains(node);
        result.scene.stations.append(station);
        stationIds.insert(key, station.id);
        result.stationPositions.insert(key, station.position);
    }
    result.statistics.stationCount = result.stationPositions.size();

    for (const TherionCentrelineShot &shot : observations.shots) {
        const auto fromId = stationIds.constFind(shot.fromKey);
        if (fromId == stationIds.constEnd()) {
            continue;
        }
        ThreeDViewerShot sceneShot;
        sceneShot.fromStationId = fromId.value();
        sceneShot.surveyId = surveyIdFor(shot.surveyPath);
        sceneShot.splay = shot.splay;
        sceneShot.duplicate = shot.duplicate;
        sceneShot.surface = shot.surface;
        if (shot.toKey.isEmpty()) {
            const ThreeDViewerVec3 base = result.scene.stations[int(fromId.value()) - 1].position;
            const ThreeDViewerVec3 delta = frame.apply(shot.delta);
            ThreeDViewerStation end;
            end.id = quint32(result.scene.stations.size() + 1);
            end.surveyId = sceneShot.surveyId;
            end.position = {base.x + delta.x, base.y + delta.y, base.z + delta.z};
            result.scene.stations.append(end);
            sceneShot.toStationId = end.id;
        } else {
            const auto toId = stationIds.constFind(shot.toKey);
            if (toId == stationIds.constEnd()) {
                continue;
            }
            sceneShot.toStationId = toId.value();
        }
        result.scene.shots.append(sceneShot);
    }
    return result;
}

} // namespace TherionStudio
