#include "TherionProjectionRules.h"

#include "TherionCommandLineModel.h"
#include "TherionSourceLogicalDocument.h"

#include <QRegularExpression>
#include <QStringList>

namespace TherionStudio
{
namespace
{
QStringList projectionWords(QString projection)
{
    projection = projection.trimmed();
    if (projection.startsWith(QLatin1Char('['))) {
        projection.remove(0, 1);
        if (projection.endsWith(QLatin1Char(']'))) {
            projection.chop(1);
        }
    }
    static const QRegularExpression whitespace(QStringLiteral(R"(\s+)"));
    return projection.split(whitespace, Qt::SkipEmptyParts);
}
}

QString therionCommandProjection(const TherionSourceLogicalCommand &command)
{
    for (const TherionSourceLogicalOptionEntryRange &optionEntry : command.optionEntryRanges) {
        const QString optionName = normalizedCommandOptionName(optionEntry.key);
        if ((optionName == QStringLiteral("projection") || optionName == QStringLiteral("proj"))
            && !optionEntry.valueRanges.isEmpty()) {
            return projectionWords(optionEntry.valueRanges.constFirst().text).join(QLatin1Char(' '));
        }
    }
    return QString();
}

QString therionScrapProjection(const TherionSourceLogicalCommand &scrapCommand)
{
    const QString projection = therionCommandProjection(scrapCommand);
    return projection.isEmpty() ? QStringLiteral("plan") : projection;
}

QString therionProjectionIdentity(const QString &projection)
{
    const QStringList words = projectionWords(projection);
    if (words.isEmpty()) {
        return QString();
    }
    const QString type = words.first().section(QLatin1Char(':'), 0, 0);
    if (type != QStringLiteral("plan") && type != QStringLiteral("extended") && type != QStringLiteral("elevation")
        && type != QStringLiteral("none")) {
        return QString();
    }
    QString identity = type + QLatin1Char(':') + words.first().section(QLatin1Char(':'), 1);
    if (type == QStringLiteral("elevation") && words.size() > 1) {
        identity += QLatin1Char(':') + words.at(1);
    }
    return identity;
}

bool therionProjectionIsCrossSection(const QString &projection)
{
    return projectionWords(projection).value(0) == QStringLiteral("none");
}
}
