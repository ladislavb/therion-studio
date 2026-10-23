#include "TherionStationPositionExport.h"

#include "ProjectStructureIndex.h"
#include "TherionSourceLogicalDocument.h"
#include "TherionSourceReferenceResolver.h"

#include <QDir>
#include <QtMath>

namespace TherionStudio
{
namespace
{
// Therion splits arguments on spaces, so a path is quoted; forward slashes work on every platform.
QString configPathArgument(const QString &path)
{
    return QStringLiteral("\"%1\"").arg(QDir::fromNativeSeparators(path));
}

// Project index station key for a station row, or empty for an unnamed station (`-`, `.`).
QString sqlStationKey(const QString &name, const QString &surveyFullName)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty() || trimmed == QStringLiteral("-") || trimmed == QStringLiteral(".")) {
        return QString();
    }
    return ProjectStructureIndex::stationKey(trimmed, surveyFullName.trimmed());
}

// Parses `count` numbers from `row` starting at `first`.
std::optional<QVector<double>> sqlNumbers(const QStringList &row, int first, int count)
{
    QVector<double> numbers;
    for (int index = first; index < first + count; ++index) {
        bool ok = false;
        numbers.append(row.at(index).toDouble(&ok));
        if (!ok) {
            return std::nullopt;
        }
    }
    return numbers;
}
}

QStringList therionConfigSourcePaths(const QString &configPath, const QString &configText)
{
    QStringList paths;
    const TherionSourceLogicalDocument document = TherionSourceLogicalDocument::fromText(configText);
    for (const TherionSourceLogicalCommand &command : document.commands()) {
        if (command.metadata.commandName != QStringLiteral("source")) {
            continue;
        }
        const QString path = resolveTherionSourceReferencePath(configPath, therionSourceReferencePathToken(command));
        if (!path.isEmpty() && !paths.contains(path)) {
            paths.append(path);
        }
    }
    return paths;
}

QString therionStationPositionExportConfig(const QStringList &sourcePaths, const QString &outputFileName)
{
    QString config = QStringLiteral("encoding utf-8\n");
    for (const QString &path : sourcePaths) {
        config += QStringLiteral("source %1\n").arg(configPathArgument(path));
    }
    config += QStringLiteral("export database -fmt sql -o %1\n").arg(configPathArgument(outputFileName));
    return config;
}

QString therionStationPositionQuery()
{
    return QStringLiteral("SELECT STATION.NAME, SURVEY.FULL_NAME, STATION.X, STATION.Y, STATION.Z "
                          "FROM STATION JOIN SURVEY ON SURVEY.ID = STATION.SURVEY_ID");
}

QHash<QString, Th2StationPosition> therionStationPositionsFromRows(const QVector<QStringList> &rows)
{
    QHash<QString, Th2StationPosition> positions;
    for (const QStringList &row : rows) {
        if (row.size() < 5) {
            continue;
        }
        const QString key = sqlStationKey(row.at(0), row.at(1));
        const std::optional<QVector<double>> xyz = sqlNumbers(row, 2, 3);
        if (!key.isEmpty() && xyz.has_value()) {
            positions.insert(key, Th2StationPosition{xyz->at(0), xyz->at(1), xyz->at(2)});
        }
    }
    return positions;
}

QString therionMeasuredLegQuery()
{
    return QStringLiteral(
        "SELECT FROM_STATION.NAME, FROM_SURVEY.FULL_NAME, TO_STATION.NAME, TO_SURVEY.FULL_NAME, "
        "SHOT.LENGTH, SHOT.BEARING, SHOT.GRADIENT FROM SHOT "
        "JOIN STATION FROM_STATION ON FROM_STATION.ID = SHOT.FROM_ID "
        "JOIN SURVEY FROM_SURVEY ON FROM_SURVEY.ID = FROM_STATION.SURVEY_ID "
        "JOIN STATION TO_STATION ON TO_STATION.ID = SHOT.TO_ID "
        "JOIN SURVEY TO_SURVEY ON TO_SURVEY.ID = TO_STATION.SURVEY_ID "
        "WHERE SHOT.ID NOT IN (SELECT SHOT_ID FROM SHOT_FLAG WHERE FLAG IN ('spl', 'dpl'))");
}

QVector<Th2MeasuredLeg> therionMeasuredLegsFromRows(const QVector<QStringList> &rows)
{
    QVector<Th2MeasuredLeg> legs;
    for (const QStringList &row : rows) {
        if (row.size() < 7) {
            continue;
        }
        const QString fromKey = sqlStationKey(row.at(0), row.at(1));
        const QString toKey = sqlStationKey(row.at(2), row.at(3));
        const std::optional<QVector<double>> shot = sqlNumbers(row, 4, 3);
        if (fromKey.isEmpty() || toKey.isEmpty() || !shot.has_value()) {
            continue;
        }
        const double length = shot->at(0);
        const double bearing = qDegreesToRadians(shot->at(1));
        const double gradient = qDegreesToRadians(shot->at(2));
        const double horizontal = length * qCos(gradient);
        legs.append(Th2MeasuredLeg{
            fromKey, toKey, Th2StationPosition{horizontal * qSin(bearing), horizontal * qCos(bearing), length * qSin(gradient)}});
    }
    return legs;
}
}
