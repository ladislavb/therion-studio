#include "ThreeDViewerSceneModel.h"

#include <QSet>
#include <QStringList>

#include <algorithm>
#include <cmath>

namespace TherionStudio
{

void ThreeDViewerBounds::include(const ThreeDViewerVec3 &point)
{
    if (!valid) {
        minimum = point;
        maximum = point;
        valid = true;
        return;
    }

    minimum.x = std::min(minimum.x, point.x);
    minimum.y = std::min(minimum.y, point.y);
    minimum.z = std::min(minimum.z, point.z);
    maximum.x = std::max(maximum.x, point.x);
    maximum.y = std::max(maximum.y, point.y);
    maximum.z = std::max(maximum.z, point.z);
}

ThreeDViewerBounds ThreeDViewerSceneModel::bounds() const
{
    ThreeDViewerBounds result;

    for (const ThreeDViewerStation &station : stations) {
        result.include(station.position);
    }

    for (const ThreeDViewerMeshGroup &mesh : meshGroups) {
        for (const ThreeDViewerVec3 &vertex : mesh.vertices) {
            result.include(vertex);
        }
    }
    for (const ThreeDViewerScrapPolyline &line : scrapLinework) {
        for (const ThreeDViewerVec3 &point : line.points) {
            result.include(point);
        }
    }
    for (const ThreeDViewerScrapPoint &point : scrapPoints) {
        result.include(point.position);
    }

    for (const ThreeDViewerSurface &surface : surfaces) {
        if (surface.width == 0 || surface.height == 0) {
            continue;
        }
        const qsizetype expectedSize = qsizetype(surface.width) * qsizetype(surface.height);
        if (surface.elevations.size() < expectedSize) {
            continue;
        }
        qsizetype index = 0;
        for (quint32 row = 0; row < surface.height; ++row) {
            for (quint32 column = 0; column < surface.width; ++column) {
                result.include({surface.calibration[0] + surface.calibration[2] * double(column) + surface.calibration[3] * double(row),
                                surface.calibration[1] + surface.calibration[4] * double(column) + surface.calibration[5] * double(row),
                                surface.elevations[index]});
                ++index;
            }
        }
    }

    return result;
}

bool ThreeDViewerSceneModel::isEmpty() const
{
    return surveys.isEmpty() && stations.isEmpty() && shots.isEmpty() && meshGroups.isEmpty()
        && scrapLinework.isEmpty() && scrapPoints.isEmpty() && surfaces.isEmpty();
}

QString ThreeDViewerSceneModel::surveyPathForId(quint32 surveyId) const
{
    if (surveyId == 0) {
        return {};
    }

    QStringList components;
    QSet<quint32> visited;
    quint32 currentSurveyId = surveyId;
    for (int guard = 0; guard < 64 && currentSurveyId != 0; ++guard) {
        if (visited.contains(currentSurveyId)) {
            break;
        }
        visited.insert(currentSurveyId);

        const ThreeDViewerSurvey *survey = nullptr;
        for (const ThreeDViewerSurvey &candidate : surveys) {
            if (candidate.id == currentSurveyId) {
                survey = &candidate;
                break;
            }
        }
        if (survey == nullptr) {
            break;
        }

        const QString surveyName = survey->name.isEmpty() ? survey->title : survey->name;
        if (surveyName.isEmpty()) {
            break;
        }

        components.append(surveyName);
        currentSurveyId = survey->parentId;
    }

    return components.join(QLatin1Char('.'));
}

QString ThreeDViewerSceneModel::stationQualifiedName(const ThreeDViewerStation &station) const
{
    const QString surveyPath = surveyPathForId(station.surveyId);
    if (surveyPath.isEmpty()) {
        return station.name;
    }
    if (station.name.isEmpty()) {
        return QStringLiteral("@%1").arg(surveyPath);
    }
    return QStringLiteral("%1@%2").arg(station.name, surveyPath);
}

void ThreeDViewerMeshGroup::computeNormals()
{
    normals.clear();
    if (vertices.isEmpty()) {
        return;
    }

    normals.resize(vertices.size());
    for (auto &n : normals) {
        n = {0.0, 0.0, 0.0};
    }

    for (const auto &triangle : triangles) {
        if (triangle[0] >= quint32(vertices.size())
            || triangle[1] >= quint32(vertices.size())
            || triangle[2] >= quint32(vertices.size())) {
            continue;
        }

        const ThreeDViewerVec3 &v0 = vertices.at(triangle[0]);
        const ThreeDViewerVec3 &v1 = vertices.at(triangle[1]);
        const ThreeDViewerVec3 &v2 = vertices.at(triangle[2]);

        const double e1x = v1.x - v0.x;
        const double e1y = v1.y - v0.y;
        const double e1z = v1.z - v0.z;
        const double e2x = v2.x - v0.x;
        const double e2y = v2.y - v0.y;
        const double e2z = v2.z - v0.z;

        double nx = e1y * e2z - e1z * e2y;
        double ny = e1z * e2x - e1x * e2z;
        double nz = e1x * e2y - e1y * e2x;
        const double lenSq = nx * nx + ny * ny + nz * nz;
        if (lenSq > 1e-12) {
            const double invLen = 1.0 / std::sqrt(lenSq);
            nx *= invLen;
            ny *= invLen;
            nz *= invLen;
        } else {
            nx = 0.0;
            ny = 0.0;
            nz = 1.0;
        }

        normals[triangle[0]].x += nx;
        normals[triangle[0]].y += ny;
        normals[triangle[0]].z += nz;

        normals[triangle[1]].x += nx;
        normals[triangle[1]].y += ny;
        normals[triangle[1]].z += nz;

        normals[triangle[2]].x += nx;
        normals[triangle[2]].y += ny;
        normals[triangle[2]].z += nz;
    }

    for (auto &n : normals) {
        const double lenSq = n.x * n.x + n.y * n.y + n.z * n.z;
        if (lenSq > 1e-12) {
            const double invLen = 1.0 / std::sqrt(lenSq);
            n.x *= invLen;
            n.y *= invLen;
            n.z *= invLen;
        } else {
            n = {0.0, 0.0, 1.0};
        }
    }
}

} // namespace TherionStudio
