#pragma once

#include <QVector>
#include <QString>
#include <QPointF>

#include <array>

namespace TherionStudio
{

struct ThreeDViewerVec3
{
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct ThreeDViewerBounds
{
    bool valid = false;
    ThreeDViewerVec3 minimum;
    ThreeDViewerVec3 maximum;

    void include(const ThreeDViewerVec3 &point);
};

struct ThreeDViewerSurvey
{
    quint32 id = 0;
    quint32 parentId = 0;
    QString name;
    QString title;
};

struct ThreeDViewerStation
{
    quint32 id = 0;
    quint32 surveyId = 0;
    QString name;
    QString comment;
    ThreeDViewerVec3 position;
    bool surface = false;
    bool entrance = false;
    bool fixed = false;
    bool continuation = false;
    bool hasWalls = false;
};

enum class ThreeDViewerShotSection
{
    None = 0,
    Oval,
    Square,
    Diamond,
    Tunnel,
};

struct ThreeDViewerShot
{
    quint32 fromStationId = 0;
    quint32 toStationId = 0;
    quint32 surveyId = 0;
    std::array<double, 4> fromLrud = {};
    std::array<double, 4> toLrud = {};
    double threshold = 0.0;
    bool surface = false;
    bool duplicate = false;
    bool hidden = false;
    bool notLrud = false;
    bool splay = false;
    ThreeDViewerShotSection section = ThreeDViewerShotSection::None;
};

struct ThreeDViewerScrapPolyline
{
    QString type;
    QString subtype;
    QVector<ThreeDViewerVec3> points;
    bool closed = false;
    bool strokeVisible = true;
    quint32 strokeColorRgba = 0;
    bool hasStrokeColor = false;
    qreal strokeWidth = 1.0;
    Qt::PenStyle penStyle = Qt::SolidLine;
    QVector<qreal> dashPattern;
    QString sourceId;
};

struct ThreeDViewerScrapPoint
{
    QString type;
    QString subtype;
    ThreeDViewerVec3 position;
    quint32 fillColorRgba = 0;
    quint32 strokeColorRgba = 0;
    bool hasFillColor = false;
    bool hasStrokeColor = false;
    qreal size = 4.0;
    qreal strokeWidth = 1.0;
    QVector<QVector<QPointF>> symbolStrokes;
    QString text;
    double orientationDegrees = 0.0;
};

struct ThreeDViewerMeshGroup
{
    quint32 id = 0;
    quint32 surveyId = 0;
    QVector<ThreeDViewerVec3> vertices;
    QVector<std::array<quint32, 3>> triangles;
    QVector<ThreeDViewerVec3> normals;
    quint32 colorRgba = 0;
    bool hasColor = false;

    void computeNormals();
};

struct ThreeDViewerSurface
{
    quint32 id = 0;
    quint32 width = 0;
    quint32 height = 0;
    std::array<double, 6> calibration = {};
    QVector<double> elevations;
};

struct ThreeDViewerSceneModel
{
    QVector<ThreeDViewerSurvey> surveys;
    QVector<ThreeDViewerStation> stations;
    QVector<ThreeDViewerShot> shots;
    QVector<ThreeDViewerMeshGroup> meshGroups;
    QVector<ThreeDViewerScrapPolyline> scrapLinework;
    QVector<ThreeDViewerScrapPoint> scrapPoints;
    QVector<ThreeDViewerSurface> surfaces;

    ThreeDViewerBounds bounds() const;
    bool isEmpty() const;
    QString surveyPathForId(quint32 surveyId) const;
    QString stationQualifiedName(const ThreeDViewerStation &station) const;
};

} // namespace TherionStudio
