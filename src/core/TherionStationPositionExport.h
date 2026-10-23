#pragma once

#include "Th2CentrelineAlignment.h"

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

namespace TherionStudio
{
// Absolute paths of the `source` commands in a Therion config (the survey data it compiles).
[[nodiscard]] QStringList therionConfigSourcePaths(const QString &configPath, const QString &configText);

// A config that compiles `sourcePaths` and only exports the database (`export database -fmt sql`) to
// `outputFileName`, relative to the folder Therion runs in.
[[nodiscard]] QString therionStationPositionExportConfig(const QStringList &sourcePaths, const QString &outputFileName);

// Query for the station positions in an imported Therion SQL export: name, survey full name, X, Y, Z.
[[nodiscard]] QString therionStationPositionQuery();

// Station positions by project index station key, from rows of therionStationPositionQuery(); unnamed
// stations (`-`, `.`) and rows that do not parse are skipped.
[[nodiscard]] QHash<QString, Th2StationPosition> therionStationPositionsFromRows(const QVector<QStringList> &rows);

// Query for the measured legs in an imported Therion SQL export, splays and duplicates left out: from name, from
// survey full name, to name, to survey full name, length, bearing, gradient. These are the values before loop
// closure, with calibration and declination applied.
[[nodiscard]] QString therionMeasuredLegQuery();

// Measured legs from rows of therionMeasuredLegQuery(); legs to unnamed stations and rows that do not parse are
// skipped. Therion also lists `nosurvey` legs as shots, with the vector computed from the network; they cannot be
// told apart here, and rarely join two stations of one scrap.
[[nodiscard]] QVector<Th2MeasuredLeg> therionMeasuredLegsFromRows(const QVector<QStringList> &rows);
}
