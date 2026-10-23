#include "TherionCompileDatabaseFigures.h"

#include "TherionSqlReportDatabase.h"

#include <limits>

namespace TherionStudio
{
namespace
{
constexpr int kNoRowLimit = std::numeric_limits<int>::max();
constexpr int kMapItemNormal = 4;

std::optional<double> optionalNumber(const QString &text)
{
    bool ok = false;
    const double value = text.toDouble(&ok);
    return ok ? std::optional<double>(value) : std::nullopt;
}

TherionSqlReportTable queryRows(const TherionSqlReportDatabase &database, const QString &query)
{
    QString errorMessage;
    return database.executeReportQuery(query, &errorMessage, kNoRowLimit);
}
}

QString therionCompiledObjectKey(const QString &name, const QString &surveyFullName)
{
    return surveyFullName.isEmpty() ? name : QStringLiteral("%1@%2").arg(name, surveyFullName);
}

TherionCompileDatabaseFigures readTherionCompileDatabaseFigures(const QStringList &databasePaths)
{
    TherionCompileDatabaseFigures figures;
    TherionSqlReportDatabase database;
    for (const QString &path : databasePaths) {
        if (database.importFile(path).success) {
            figures.databasePath = path;
            break;
        }
    }
    if (!figures.isAvailable()) {
        return figures;
    }

    // Item targets by Therion object ID; maps and scraps share one ID space.
    QHash<QString, TherionMapItemFigures> itemTargetsById;
    QHash<QString, QString> mapKeysById;
    const TherionSqlReportTable maps = queryRows(database, QStringLiteral(
        "select M.ID, M.NAME, S.FULL_NAME, M.TITLE, M.LENGTH, M.DEPTH "
        "from MAPS M left join SURVEY S on S.ID = M.SURVEY_ID"));
    for (const QStringList &row : maps.rows) {
        TherionMapFigures map;
        map.title = row.at(3);
        map.length = optionalNumber(row.at(4));
        if (const std::optional<double> depth = optionalNumber(row.at(5))) {
            map.depth = 0.0 - *depth; // not -0.0 for a flat map
        }
        const QString key = therionCompiledObjectKey(row.at(1), row.at(2));
        figures.mapsByKey.insert(key, map);
        mapKeysById.insert(row.at(0), key);
        itemTargetsById.insert(row.at(0), {kMapItemNormal, true, row.at(1), row.at(2)});
    }

    const TherionSqlReportTable scraps = queryRows(database, QStringLiteral(
        "select C.ID, C.NAME, S.FULL_NAME, C.AVG_DISTORTION, C.MAX_DISTORTION "
        "from SCRAPS C left join SURVEY S on S.ID = C.SURVEY_ID"));
    for (const QStringList &row : scraps.rows) {
        figures.scrapsByKey.insert(therionCompiledObjectKey(row.at(1), row.at(2)),
                                   {optionalNumber(row.at(3)), optionalNumber(row.at(4))});
        itemTargetsById.insert(row.at(0), {kMapItemNormal, false, row.at(1), row.at(2)});
    }

    const TherionSqlReportTable items = queryRows(database, QStringLiteral(
        "select ID, TYPE, ITEMID from MAPITEMS order by rowid"));
    for (const QStringList &row : items.rows) {
        const auto map = figures.mapsByKey.find(mapKeysById.value(row.at(0)));
        const auto target = itemTargetsById.constFind(row.at(2));
        if (map == figures.mapsByKey.end() || target == itemTargetsById.constEnd()) {
            continue;
        }
        TherionMapItemFigures item = target.value();
        item.type = row.at(1).toInt();
        map->items.append(item);
    }
    return figures;
}
}
