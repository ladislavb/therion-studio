#pragma once

#include <QHash>
#include <QLineF>
#include <QPointF>
#include <QString>
#include <QVector>

namespace TherionStudio
{
struct TherionXviSketchLine
{
    QString colorToken;
    QVector<QPointF> points;
};

struct TherionXviStation
{
    QString name;
    QPointF position;
};

struct TherionXviShot
{
    QLineF centerLine;
    // Therion appends four LRUD-derived passage-outline vertices to an XVI shot.
    // They are retained in the order used by XTherion: from-left, to-left,
    // to-right, from-right.
    QVector<QPointF> passageOutline;
};

struct TherionXviDocument
{
    QPointF gridOrigin;
    bool hasGridOrigin = false;
    QPointF gridVectorX;
    QPointF gridVectorY;
    int gridCountX = 0;
    int gridCountY = 0;
    bool hasGridDefinition = false;
    QVector<TherionXviStation> stationEntries;
    QHash<QString, QPointF> stations;
    QVector<TherionXviShot> shots;
    QVector<TherionXviSketchLine> sketchLines;
};

bool parseTherionXviDocumentText(const QString &content, TherionXviDocument *document);
bool parseTherionXviDocumentFile(const QString &xviPath, TherionXviDocument *document);
}
