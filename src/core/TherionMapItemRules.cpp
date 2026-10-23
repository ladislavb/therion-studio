#include "TherionMapItemRules.h"

#include "TherionSourceLogicalDocument.h"

#include <QRegularExpression>

namespace TherionStudio
{
namespace
{
bool isShiftOrPreviewType(const QString &value, bool allowNone)
{
    return value == QStringLiteral("above")
        || value == QStringLiteral("below")
        || (allowNone && value == QStringLiteral("none"));
}

bool isNumber(const QString &value)
{
    bool ok = false;
    value.toDouble(&ok);
    return ok;
}

// Therion has already removed the brackets when it reads the shift.
bool isValidShift(QString shift)
{
    if (shift.startsWith(QLatin1Char('['))) {
        shift.remove(0, 1);
        if (shift.endsWith(QLatin1Char(']'))) {
            shift.chop(1);
        }
    }
    static const QRegularExpression whitespace(QStringLiteral(R"(\s+)"));
    const QStringList words = shift.split(whitespace, Qt::SkipEmptyParts);
    return words.size() >= 2 && words.size() <= 3 && isNumber(words.at(0)) && isNumber(words.at(1));
}
}

TherionMapItemSyntax therionMapItemSyntax(const QStringList &values, bool mapsOffset)
{
    TherionMapItemSyntax syntax;
    syntax.name = values.value(0).trimmed();
    if (values.size() != 1 && values.size() != 3) {
        syntax.problem = TherionMapItemProblem::InvalidItem;
        return syntax;
    }
    if (values.size() == 1 || !mapsOffset) {
        return syntax;
    }
    if (!isValidShift(values.at(1))) {
        syntax.problem = TherionMapItemProblem::InvalidShift;
        syntax.problemValueIndex = 1;
    } else if (!isShiftOrPreviewType(values.at(2), true)) {
        syntax.problem = TherionMapItemProblem::UnsupportedPreviewType;
        syntax.problemValueIndex = 2;
    }
    return syntax;
}

TherionMapItemSyntax therionMapPreviewSyntax(const QStringList &values)
{
    TherionMapItemSyntax syntax;
    syntax.name = values.value(2).trimmed();
    if (!isShiftOrPreviewType(values.value(1), false)) {
        syntax.problem = TherionMapItemProblem::UnsupportedPreviewType;
        syntax.problemValueIndex = 1;
    }
    return syntax;
}

bool therionConfigUsesMapsOffset(const TherionSourceLogicalDocument &configDocument)
{
    bool mapsOffset = true;
    for (const TherionSourceLogicalCommand &command : configDocument.commands()) {
        if (command.parsed.directive != QStringLiteral("maps-offset")) {
            continue;
        }
        const QString value = command.parsed.tokens.value(1);
        if (value == QStringLiteral("on")) {
            mapsOffset = true;
        } else if (value == QStringLiteral("off")) {
            mapsOffset = false;
        }
    }
    return mapsOffset;
}
}
