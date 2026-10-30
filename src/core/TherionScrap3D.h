#pragma once

#include "ThreeDViewerSceneModel.h"
#include "Th2GeometryProjection.h"

#include <QPointF>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

namespace TherionStudio
{

struct TherionScrapAnchor
{
    QString stationName;               // Raw name from .th2 (e.g. "0@hp" or "0")
    QString resolvedStationKey;        // Matched key in stationPositions (e.g. "0@hp.1302")
    QPointF scrapPoint;                // 2D drawing point in .th2 scrap coordinates
    ThreeDViewerVec3 centrelinePosition; // 3D station position from centreline solver
    double residual = 0.0;             // Horizontal distance after fit (meters)
    bool matched = false;
};

struct TherionScrapScale
{
    bool valid = false;
    QPointF p1;
    QPointF p2;
    double worldLengthMeters = 0.0;
    double metersPerPoint = 0.0;
};

struct TherionScrapTransform
{
    bool valid = false;
    double scale = 1.0;     // Meters per scrap point
    double rotation = 0.0;  // Rotation in radians
    double tx = 0.0;        // East offset (meters)
    double ty = 0.0;        // North offset (meters)
    double meanResidual = 0.0;
    double maxResidual = 0.0;
    int anchorCount = 0;
    int matchedAnchorCount = 0;
    bool approximate = false;

    [[nodiscard]] QPointF transformPoint(const QPointF &scrapPoint) const;
    [[nodiscard]] double interpolateZ(const QPointF &scrapPoint,
                                      const QVector<TherionScrapAnchor> &matchedAnchors) const;
};

struct TherionScrap3DModel
{
    QString scrapId;
    QString projection;
    QString filePath;
    QString surveyPath;
    TherionScrapTransform transform;
    QVector<TherionScrapAnchor> anchors;
    QStringList diagnostics;
    ThreeDViewerMeshGroup meshGroup;
    QVector<QVector<ThreeDViewerVec3>> wallPolylines;
    QVector<ThreeDViewerScrapPolyline> linework;
    QVector<ThreeDViewerScrapPoint> pointFeatures;
};


struct TherionScrapJoin
{
    QString firstReference;
    QString secondReference;
    QString surveyPath;
    QString filePath;
    int lineNumber = 0;
    int count = 1;
};

struct TherionScrapJoinDiagnostic
{
    QString filePath;
    int lineNumber = 0;
    QString message;
};

struct TherionScrap3DOptions
{
    QSet<int> hiddenObjectLines;
    bool wallsOnly = false;
    bool objectsOnly = false;
};

class TherionScrap3D
{
public:
    // Parses -scale option tokens from scrap command
    [[nodiscard]] static TherionScrapScale parseScaleOption(const QStringList &tokens);

    // Fits 2D similarity transform between scrap anchors and 3D centreline stations
    [[nodiscard]] static TherionScrapTransform fitTransform(
        const QVector<TherionScrapAnchor> &anchors,
        const TherionScrapScale &scaleOption = {});

    // Resolves a station name from .th2 against available centreline station keys
    [[nodiscard]] static QString resolveStationKey(
        const QString &stationName,
        const QString &scrapSurveyPath,
        const QHash<QString, ThreeDViewerVec3> &stationPositions);

    // Drapes a scrap projection onto the 3D centreline
    [[nodiscard]] static TherionScrap3DModel projectScrap(
        const Th2GeometryProjection &th2Projection,
        const QString &scrapId,
        const QString &scrapSurveyPath,
        const QHash<QString, ThreeDViewerVec3> &stationPositions,
        quint32 meshId = 0,
        quint32 surveyId = 0,
        const TherionScrap3DOptions &options = {});

    // Parses a .th2 file and projects all plan scraps in it onto the centreline
    [[nodiscard]] static QVector<TherionScrap3DModel> projectTh2File(
        const QString &filePath,
        const QString &th2Contents,
        const QString &surveyPath,
        const QHash<QString, ThreeDViewerVec3> &stationPositions,
        quint32 startMeshId = 0,
        quint32 surveyId = 0,
        const TherionScrap3DOptions &options = {});


    [[nodiscard]] static QVector<TherionScrapJoinDiagnostic> applyJoins(
        QVector<TherionScrap3DModel> *models,
        const QVector<TherionScrapJoin> &joins);
};

} // namespace TherionStudio
