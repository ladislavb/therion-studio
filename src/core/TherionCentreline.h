#pragma once

#include "ThreeDViewerSceneModel.h"
#include "TherionScrap3D.h"

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>

namespace TherionStudio
{

// Computes cave centreline geometry directly from Therion .th survey data,
// without running Therion. Station keys use Therion's qualified form
// "name@innermost.outer" (or just "name" outside any survey).

struct TherionCentrelineDiagnostic
{
    QString filePath;
    int lineNumber = 0;
    QString message;
};

struct TherionCentrelineShot
{
    QString fromKey;
    QString toKey; // empty for an anonymous splay end
    ThreeDViewerVec3 delta;
    double length = 0.0;
    bool splay = false;
    bool duplicate = false;
    bool surface = false;
    QString surveyPath;
    QString filePath;
    int lineNumber = 0;
};

struct TherionCentrelineFix
{
    QString stationKey;
    ThreeDViewerVec3 position;
    QString coordinateSystem;
    QString filePath;
    int lineNumber = 0;
};

struct TherionCentrelineTh2Reference
{
    QString filePath;
    QString surveyPath;
};

struct TherionCentrelineObservations
{
    QVector<TherionCentrelineShot> shots;
    QVector<TherionCentrelineFix> fixes;
    QVector<QStringList> equates;
    QStringList surveyPaths; // innermost-first, dot separated
    QStringList stationKeysWithoutGeometry; // nosurvey-only stations
    QStringList files;
    QVector<TherionCentrelineTh2Reference> th2References;
    QVector<TherionScrapJoin> joins;
    QVector<TherionCentrelineDiagnostic> diagnostics;
};

struct TherionCentrelineStatistics
{
    int stationCount = 0;
    int shotCount = 0;
    int splayCount = 0;
    int loopCount = 0;
    int componentCount = 0;
    int componentsWithoutFix = 0;
    double totalLength = 0.0;
    double maxAdjustment = 0.0;
    double meanAdjustment = 0.0;
    double fixFrameRotationDegrees = 0.0;
    bool fixFrameMirrored = false;
    double fixFrameResidual = 0.0;
};

struct TherionCentrelineResult
{
    ThreeDViewerSceneModel scene;
    QHash<QString, ThreeDViewerVec3> stationPositions;
    QVector<TherionScrap3DModel> scrapModels;
    TherionCentrelineStatistics statistics;
    QVector<TherionCentrelineDiagnostic> diagnostics;
};

using TherionCentrelineFileReader = std::function<bool(const QString &filePath, QString *contents)>;

// Reads files with DocumentFile (honours Therion "encoding").
TherionCentrelineFileReader defaultTherionCentrelineFileReader();

// Follows "input" (and "source" in .thconfig) from the entry files.
TherionCentrelineObservations parseTherionCentreline(const QStringList &entryFilePaths,
                                                     const TherionCentrelineFileReader &readFile);

// Least-squares loop adjustment. Shots are measured against true north; when
// the network has two or more fixed stations the horizontal rotation (and a
// possible axis mirror) of the fix coordinate system is estimated from them.
TherionCentrelineResult solveTherionCentreline(const TherionCentrelineObservations &observations);

TherionCentrelineResult computeTherionCentreline(const QStringList &entryFilePaths,
                                                 const TherionCentrelineFileReader &readFile);

} // namespace TherionStudio
