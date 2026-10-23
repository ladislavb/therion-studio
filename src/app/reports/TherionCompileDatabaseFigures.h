#pragma once

#include <QHash>
#include <QString>
#include <QVector>

#include <optional>

namespace TherionStudio
{
// One entry of a map in Therion's MAPITEMS table.
struct TherionMapItemFigures
{
    // Therion map item type (thdb2dmi.h): 1 below, 2 above, 3 beside, 4 normal, 5 none.
    int type = 0;
    bool isMap = false;
    QString name;
    QString surveyFullName;
};

struct TherionMapFigures
{
    QString title;
    std::optional<double> length;
    // Positive, top minus bottom as in the PDF legend (the SQL DEPTH column is bottom minus top).
    std::optional<double> depth;
    QVector<TherionMapItemFigures> items;
};

struct TherionScrapFigures
{
    std::optional<double> averageDistortion;
    std::optional<double> maximumDistortion;
};

// Map and scrap figures of a Therion SQL database export.
struct TherionCompileDatabaseFigures
{
    QString databasePath;
    QHash<QString, TherionMapFigures> mapsByKey;
    QHash<QString, TherionScrapFigures> scrapsByKey;

    [[nodiscard]] bool isAvailable() const { return !databasePath.isEmpty(); }
};

// Key of a map or scrap: its name and the full name of the survey block it is defined in.
[[nodiscard]] QString therionCompiledObjectKey(const QString &name, const QString &surveyFullName);

// Reads the figures of the first export that imports. It blocks, so call it off the UI thread.
[[nodiscard]] TherionCompileDatabaseFigures readTherionCompileDatabaseFigures(const QStringList &databasePaths);
}
