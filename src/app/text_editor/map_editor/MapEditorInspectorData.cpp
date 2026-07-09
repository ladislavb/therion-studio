#include "MapEditorInspectorData.h"

#include "MapEditorSceneSupport.h"
#include "../../../core/ProjectStructureIndex.h"
#include "../../../core/TherionCommandLineModel.h"
#include "../../../core/TherionDocumentParser.h"
#include "../../../core/TherionSourceLogicalDocument.h"
#include "../../../core/TherionTokenRules.h"

#include <QComboBox>
#include <QApplication>
#include <QFile>
#include <QHash>
#include <QIcon>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QLineEdit>
#include <QPainter>
#include <QPalette>
#include <QSvgRenderer>

#include <algorithm>

namespace TherionStudio
{
namespace
{
QVector<TherionParsedLine> parsedLinesForLogicalCommands(const QVector<TherionSourceLogicalCommand> &commands)
{
    QVector<TherionParsedLine> parsedLines;
    parsedLines.reserve(commands.size());
    for (const TherionSourceLogicalCommand &command : commands) {
        parsedLines.append(command.parsed);
    }
    return parsedLines;
}

QPixmap renderInspectorLucidePixmap(const QString &iconName, const QColor &color, int extent)
{
    QFile file(QStringLiteral(":/resources/icons/lucide/%1.svg").arg(iconName));
    if (!file.open(QIODevice::ReadOnly)) {
        return QPixmap();
    }

    QString svg = QString::fromUtf8(file.readAll());
    svg.replace(QStringLiteral("currentColor"), color.name(QColor::HexRgb));
    QSvgRenderer renderer(svg.toUtf8());
    if (!renderer.isValid()) {
        return QPixmap();
    }

    const qreal devicePixelRatio = qApp != nullptr ? qApp->devicePixelRatio() : 1.0;
    QPixmap pixmap(QSize(extent, extent) * devicePixelRatio);
    pixmap.setDevicePixelRatio(devicePixelRatio);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    renderer.render(&painter, QRectF(0, 0, extent, extent));
    return pixmap;
}
}

bool inspectorTokenLooksNumeric(const QString &token)
{
    if (token.isEmpty()) {
        return false;
    }

    bool ok = false;
    token.toDouble(&ok);
    return ok;
}

QString inspectorBracketedOptionValue(const QStringList &tokens, const QString &option)
{
    for (int index = 0; index < tokens.size(); ++index) {
        if (!commandOptionNameMatches(tokens.at(index), option)) {
            continue;
        }

        const int nextOptionIndex = nextCommandOptionIndex(tokens, index);
        if (nextOptionIndex <= index + 1) {
            return QString();
        }

        const QString firstValue = tokens.at(index + 1);
        if (!firstValue.contains(QLatin1Char('[')) || firstValue.contains(QLatin1Char(']'))) {
            return firstValue;
        }

        QStringList valueTokens;
        valueTokens.append(firstValue);
        for (int valueIndex = index + 2; valueIndex < nextOptionIndex; ++valueIndex) {
            const QString token = tokens.at(valueIndex);
            valueTokens.append(token);
            if (token.contains(QLatin1Char(']'))) {
                break;
            }
        }
        return valueTokens.join(QLatin1Char(' '));
    }

    return QString();
}

QString inspectorPointTypeToken(const TherionParsedLine &parsedLine)
{
    bool skipOptionValue = false;
    for (int index = 1; index < parsedLine.tokens.size(); ++index) {
        const QString token = parsedLine.tokens.at(index);
        if (skipOptionValue) {
            skipOptionValue = false;
            continue;
        }
        if (inspectorTokenLooksNumeric(token)) {
            continue;
        }
        if (TherionTokenRules::tokenStartsOption(token)) {
            skipOptionValue = nextCommandOptionIndex(parsedLine.tokens, index) > index + 1;
            continue;
        }

        return token;
    }

    return QString();
}

QString inspectorStationNameToken(const TherionParsedLine &parsedLine)
{
    bool skipOptionValue = false;
    for (int index = 1; index < parsedLine.tokens.size(); ++index) {
        const QString token = parsedLine.tokens.at(index);
        if (skipOptionValue) {
            skipOptionValue = false;
            continue;
        }
        if (token.startsWith(QLatin1Char('-'))) {
            skipOptionValue = index + 1 < parsedLine.tokens.size();
            continue;
        }
        if (token.compare(QStringLiteral("station"), Qt::CaseInsensitive) == 0 || inspectorTokenLooksNumeric(token)) {
            continue;
        }

        return token;
    }

    return QString();
}

QString inspectorObjectTextWithOptionalIdentifier(const QString &type, const QString &subtype, const QString &identifier)
{
    const QString normalizedType = type.trimmed();
    const QString normalizedSubtype = subtype.trimmed();
    const QString normalizedIdentifier = identifier.trimmed();

    QString text = normalizedSubtype.isEmpty()
        ? normalizedType
        : QStringLiteral("%1 %2").arg(normalizedType, normalizedSubtype);
    if (!normalizedIdentifier.isEmpty()) {
        text = QStringLiteral("%1: %2").arg(text, normalizedIdentifier);
    }

    return text;
}

QString inspectorTypePart(const QString &typeToken)
{
    return typeToken.section(QLatin1Char(':'), 0, 0).trimmed();
}

QString inspectorInlineSubtypePart(const QString &typeToken)
{
    return typeToken.section(QLatin1Char(':'), 1).trimmed();
}

QString inspectorObjectSubtype(const TherionParsedLine &parsedLine, const QString &typeToken)
{
    const QString optionSubtype = commandOptionValue(parsedLine.tokens, QStringLiteral("-subtype"));
    if (!optionSubtype.isEmpty()) {
        return optionSubtype;
    }

    return inspectorInlineSubtypePart(typeToken);
}

bool inspectorObjectUsesTextField(const QString &commandKind, const QString &type, const QString &text)
{
    const QString normalizedCommand = commandKind.trimmed().toLower();
    if (normalizedCommand != QStringLiteral("point") && normalizedCommand != QStringLiteral("line")) {
        return false;
    }

    return type.trimmed().compare(QStringLiteral("label"), Qt::CaseInsensitive) == 0
        || !text.trimmed().isEmpty();
}

QSet<QString> pointValueOptionTypes()
{
    return {
        QStringLiteral("altitude"),
        QStringLiteral("date"),
        QStringLiteral("dimensions"),
        QStringLiteral("height"),
        QStringLiteral("passage-height"),
    };
}

bool commandHasOption(const QJsonObject &commandObject, const QString &optionKey)
{
    const QString normalizedOption = optionKey.trimmed().toLower();
    const QJsonArray options = commandObject.value(QStringLiteral("options")).toArray();
    for (const QJsonValue &optionValue : options) {
        const QJsonObject optionObject = optionValue.toObject();
        if (optionObject.value(QStringLiteral("option_key")).toString().trimmed().toLower() == normalizedOption) {
            return true;
        }
    }

    return false;
}

QString inspectorMapObjectIconName(const ProjectStructureEntry &entry)
{
    if (entry.category == QStringLiteral("Scraps")) {
        return QStringLiteral("puzzle");
    }
    if (entry.category == QStringLiteral("Stations") || entry.category == QStringLiteral("Points")) {
        return QStringLiteral("locate-fixed");
    }
    if (entry.category == QStringLiteral("Lines")) {
        return QStringLiteral("spline");
    }
    if (entry.category == QStringLiteral("Areas")) {
        return QStringLiteral("pentagon");
    }

    return QString();
}

QString inspectorMapObjectItemText(const ProjectStructureEntry &entry, const TherionParsedLine *parsedLine)
{
    if (entry.category == QStringLiteral("Scraps")) {
        return entry.name;
    }

    if (parsedLine == nullptr) {
        return entry.name;
    }

    if (entry.category == QStringLiteral("Stations") || entry.category == QStringLiteral("Points")) {
        const QString pointTypeToken = inspectorPointTypeToken(*parsedLine);
        const QString pointType = entry.category == QStringLiteral("Stations")
            ? QStringLiteral("station")
            : inspectorTypePart(pointTypeToken);
        const QString pointSubtype = inspectorObjectSubtype(*parsedLine, pointTypeToken);
        const QString stationName = commandOptionValue(parsedLine->tokens, QStringLiteral("-name"));
        QString identifier = stationName.isEmpty() ? commandOptionValue(parsedLine->tokens, QStringLiteral("-id")) : stationName;
        if (entry.category == QStringLiteral("Stations") && identifier.isEmpty()) {
            identifier = inspectorStationNameToken(*parsedLine);
        }
        return inspectorObjectTextWithOptionalIdentifier(pointType.isEmpty() ? entry.name : pointType,
                                                         pointSubtype,
                                                         identifier);
    }

    if (entry.category == QStringLiteral("Lines")) {
        const QString lineTypeToken = parsedLine->tokens.value(1, entry.name);
        return inspectorObjectTextWithOptionalIdentifier(inspectorTypePart(lineTypeToken),
                                                         inspectorObjectSubtype(*parsedLine, lineTypeToken),
                                                         commandOptionValue(parsedLine->tokens, QStringLiteral("-id")));
    }

    if (entry.category == QStringLiteral("Areas")) {
        const QString areaTypeToken = parsedLine->tokens.value(1, entry.name);
        return inspectorObjectTextWithOptionalIdentifier(inspectorTypePart(areaTypeToken),
                                                         inspectorObjectSubtype(*parsedLine, areaTypeToken),
                                                         commandOptionValue(parsedLine->tokens, QStringLiteral("-id")));
    }

    return entry.name;
}

QIcon inspectorActionIcon(const QString &iconName)
{
    const QPalette palette = QApplication::palette();
    QIcon icon;
    icon.addPixmap(renderInspectorLucidePixmap(iconName, palette.color(QPalette::Text), 16), QIcon::Normal);
    icon.addPixmap(renderInspectorLucidePixmap(iconName, palette.color(QPalette::Disabled, QPalette::Text), 16), QIcon::Disabled);
    return icon;
}

qreal mapSourceUnitsPerMeterFromParsedLines(const QVector<TherionParsedLine> &parsedLines)
{
    QVector<qreal> candidates;
    for (const TherionParsedLine &parsedLine : parsedLines) {
        if (parsedLine.directive != QStringLiteral("scrap")) {
            continue;
        }

        const std::optional<qreal> sourceUnitsPerMeter = sourceUnitsPerMeterFromScrapScale(parsedLine.tokens);
        if (sourceUnitsPerMeter.has_value()) {
            candidates.append(sourceUnitsPerMeter.value());
        }
    }

    if (candidates.isEmpty()) {
        return 1.0;
    }

    std::sort(candidates.begin(), candidates.end());
    return candidates.at(candidates.size() / 2);
}

QString formatTherionScaleNumber(qreal value)
{
    if (std::fabs(value) < 1e-9) {
        return QStringLiteral("0.0");
    }

    const qreal nearestInteger = std::round(value);
    if (std::fabs(value - nearestInteger) < 1e-9) {
        return QString::number(static_cast<qlonglong>(nearestInteger));
    }

    return QString::number(value, 'g', 12);
}

QString xtherionDefaultScrapScaleOption(const QRectF &sourceBounds)
{
    QRectF bounds = sourceBounds.normalized();
    if (!bounds.isValid() || bounds.width() < 1e-6) {
        bounds = QRectF(0.0, 0.0, 1600.0, 1200.0);
    }

    const qreal left = bounds.left();
    const qreal right = bounds.right();
    const qreal top = bounds.top();
    const qreal realLengthMeters = 0.0254 * (right - left);

    return QStringLiteral("-scale [%1 %2 %3 %4 0.0 0.0 %5 0.0 m]")
        .arg(formatTherionScaleNumber(left),
             formatTherionScaleNumber(top),
             formatTherionScaleNumber(right),
             formatTherionScaleNumber(top),
             formatTherionScaleNumber(realLengthMeters));
}

QString cleanedScaleToken(QString token)
{
    token = token.trimmed();
    while (token.startsWith(QLatin1Char('['))) {
        token.remove(0, 1);
    }
    while (token.endsWith(QLatin1Char(']'))) {
        token.chop(1);
    }
    return token.trimmed();
}

bool parseInspectorScaleNumber(const QString &token, qreal *value)
{
    if (value == nullptr) {
        return false;
    }

    const QString cleaned = cleanedScaleToken(token);
    if (cleaned.isEmpty()) {
        return false;
    }

    bool ok = false;
    const qreal parsed = cleaned.toDouble(&ok);
    if (!ok) {
        return false;
    }

    *value = parsed;
    return true;
}

QString normalizedInspectorScaleUnit(QString token)
{
    token = cleanedScaleToken(token).toLower();
    if (token == QStringLiteral("meter") || token == QStringLiteral("meters") || token == QStringLiteral("metre") || token == QStringLiteral("metres")) {
        return QStringLiteral("m");
    }
    if (token == QStringLiteral("centimeter") || token == QStringLiteral("centimeters") || token == QStringLiteral("centimetre") || token == QStringLiteral("centimetres")) {
        return QStringLiteral("cm");
    }
    if (token == QStringLiteral("millimeter") || token == QStringLiteral("millimeters") || token == QStringLiteral("millimetre") || token == QStringLiteral("millimetres")) {
        return QStringLiteral("mm");
    }
    if (token == QStringLiteral("foot") || token == QStringLiteral("feet")) {
        return QStringLiteral("ft");
    }
    if (token == QStringLiteral("inch") || token == QStringLiteral("inches")) {
        return QStringLiteral("in");
    }
    if (token == QStringLiteral("m") || token == QStringLiteral("cm") || token == QStringLiteral("mm") || token == QStringLiteral("ft") || token == QStringLiteral("in")) {
        return token;
    }
    return QStringLiteral("m");
}

std::optional<InspectorScrapScale> inspectorScrapScaleFromTokens(const QStringList &tokens)
{
    const int scaleOptionIndex = tokens.indexOf(QStringLiteral("-scale"));
    if (scaleOptionIndex < 0 || scaleOptionIndex + 1 >= tokens.size()) {
        return std::nullopt;
    }

    QVector<qreal> numbers;
    numbers.reserve(8);
    QString unitToken = QStringLiteral("m");
    const bool bracketed = tokens.at(scaleOptionIndex + 1).contains(QLatin1Char('['));
    for (int index = scaleOptionIndex + 1; index < tokens.size(); ++index) {
        const QString token = tokens.at(index);
        if (!bracketed && token.startsWith(QLatin1Char('-')) && !inspectorTokenLooksNumeric(token)) {
            break;
        }

        qreal parsedNumber = 0.0;
        if (parseInspectorScaleNumber(token, &parsedNumber)) {
            numbers.append(parsedNumber);
        } else {
            unitToken = normalizedInspectorScaleUnit(token);
        }

        if (!bracketed || token.contains(QLatin1Char(']'))) {
            break;
        }
    }

    if (numbers.size() < 8) {
        return std::nullopt;
    }

    InspectorScrapScale scale;
    scale.sourcePoint1 = QPointF(numbers.at(0), numbers.at(1));
    scale.sourcePoint2 = QPointF(numbers.at(2), numbers.at(3));
    scale.realPoint1 = QPointF(numbers.at(4), numbers.at(5));
    scale.realPoint2 = QPointF(numbers.at(6), numbers.at(7));
    scale.unitToken = unitToken;
    return scale;
}

QString scrapScaleExpression(const InspectorScrapScale &scale)
{
    return QStringLiteral("[%1 %2 %3 %4 %5 %6 %7 %8 %9]")
        .arg(formatTherionScaleNumber(scale.sourcePoint1.x()),
             formatTherionScaleNumber(scale.sourcePoint1.y()),
             formatTherionScaleNumber(scale.sourcePoint2.x()),
             formatTherionScaleNumber(scale.sourcePoint2.y()),
             formatTherionScaleNumber(scale.realPoint1.x()),
             formatTherionScaleNumber(scale.realPoint1.y()),
             formatTherionScaleNumber(scale.realPoint2.x()),
             formatTherionScaleNumber(scale.realPoint2.y()),
             scale.unitToken.isEmpty() ? QStringLiteral("m") : scale.unitToken);
}

InspectorScrapScale defaultInspectorScrapScale(const QRectF &sourceBounds)
{
    QRectF bounds = sourceBounds.normalized();
    if (!bounds.isValid() || bounds.width() < 1e-6) {
        bounds = QRectF(0.0, 0.0, 1600.0, 1200.0);
    }

    InspectorScrapScale scale;
    scale.sourcePoint1 = QPointF(bounds.left(), bounds.top());
    scale.sourcePoint2 = QPointF(bounds.right(), bounds.top());
    scale.realPoint1 = QPointF(0.0, 0.0);
    scale.realPoint2 = QPointF((bounds.right() - bounds.left()) * 0.0254, 0.0);
    scale.unitToken = QStringLiteral("m");
    return scale;
}

std::optional<InspectorScrapContext> inspectorScrapContextForSourceLine(const QVector<TherionParsedLine> &parsedLines,
                                                                        int lineNumber)
{
    if (lineNumber <= 0) {
        return std::nullopt;
    }

    QVector<InspectorScrapContext> scrapStack;
    for (const TherionParsedLine &parsedLine : parsedLines) {
        const QString directive = parsedLine.directive.trimmed().toLower();
        if (directive == QStringLiteral("scrap")) {
            InspectorScrapContext context;
            context.identifier = parsedLine.tokens.value(1, QStringLiteral("Unnamed Scrap"));
            context.lineNumber = parsedLine.lineNumber;
            scrapStack.append(context);
        }

        if (parsedLine.lineNumber == lineNumber) {
            if (scrapStack.isEmpty()) {
                return std::nullopt;
            }
            return scrapStack.last();
        }

        if (directive == QStringLiteral("endscrap") && !scrapStack.isEmpty()) {
            scrapStack.removeLast();
        }
    }

    return std::nullopt;
}

std::optional<InspectorScrapContext> inspectorScrapContextForSourceLine(
    const QVector<TherionSourceLogicalCommand> &commands,
    int lineNumber)
{
    return inspectorScrapContextForSourceLine(parsedLinesForLogicalCommands(commands), lineNumber);
}

InspectorScrapContext inspectorDraftInsertionScrapContext(const QVector<TherionParsedLine> &parsedLines)
{
    QVector<InspectorScrapContext> scrapStack;
    std::optional<InspectorScrapContext> lastClosedScrap;

    for (const TherionParsedLine &parsedLine : parsedLines) {
        const QString directive = parsedLine.directive.trimmed().toLower();
        if (directive == QStringLiteral("scrap")) {
            InspectorScrapContext context;
            context.identifier = parsedLine.tokens.value(1, QStringLiteral("Unnamed Scrap"));
            context.lineNumber = parsedLine.lineNumber;
            scrapStack.append(context);
        } else if (directive == QStringLiteral("endscrap") && !scrapStack.isEmpty()) {
            lastClosedScrap = scrapStack.takeLast();
        }
    }

    if (lastClosedScrap.has_value()) {
        return lastClosedScrap.value();
    }
    if (!scrapStack.isEmpty()) {
        return scrapStack.last();
    }

    InspectorScrapContext createdContext;
    createdContext.identifier = QStringLiteral("scrap-1");
    createdContext.willBeCreated = true;
    return createdContext;
}

InspectorScrapContext inspectorDraftInsertionScrapContext(const QVector<TherionSourceLogicalCommand> &commands)
{
    return inspectorDraftInsertionScrapContext(parsedLinesForLogicalCommands(commands));
}

QVector<InspectorScrapContext> inspectorScrapContexts(const QVector<TherionParsedLine> &parsedLines)
{
    QVector<InspectorScrapContext> contexts;
    for (const TherionParsedLine &parsedLine : parsedLines) {
        if (parsedLine.directive.trimmed().toLower() != QStringLiteral("scrap")) {
            continue;
        }

        InspectorScrapContext context;
        context.identifier = parsedLine.tokens.value(1).trimmed();
        if (context.identifier.isEmpty()) {
            continue;
        }
        context.lineNumber = parsedLine.lineNumber;
        contexts.append(context);
    }
    return contexts;
}

QVector<InspectorScrapContext> inspectorScrapContexts(const QVector<TherionSourceLogicalCommand> &commands)
{
    return inspectorScrapContexts(parsedLinesForLogicalCommands(commands));
}

struct InspectorCatalogCommandEntry
{
    QString key;
    QJsonObject object;
};

QVector<InspectorCatalogCommandEntry> inspectorCatalogCommandEntries(const QJsonObject &catalogObject)
{
    QVector<InspectorCatalogCommandEntry> entries;
    const QJsonValue commandsValue = catalogObject.value(QStringLiteral("commands"));
    if (commandsValue.isArray()) {
        const QJsonArray commandsArray = commandsValue.toArray();
        entries.reserve(commandsArray.size());
        for (const QJsonValue &commandValue : commandsArray) {
            const QJsonObject commandObject = commandValue.toObject();
            if (commandObject.isEmpty()) {
                continue;
            }

            InspectorCatalogCommandEntry entry;
            entry.object = commandObject;
            entry.key = commandObject.value(QStringLiteral("name")).toString();
            if (entry.key.trimmed().isEmpty()) {
                entry.key = commandObject.value(QStringLiteral("directive")).toString();
            }
            entries.append(entry);
        }
        return entries;
    }

    if (commandsValue.isObject()) {
        const QJsonObject commandsObject = commandsValue.toObject();
        entries.reserve(commandsObject.size());
        for (auto it = commandsObject.begin(); it != commandsObject.end(); ++it) {
            if (!it.value().isObject()) {
                continue;
            }

            InspectorCatalogCommandEntry entry;
            entry.key = it.key();
            entry.object = it.value().toObject();
            entries.append(entry);
        }
    }

    return entries;
}

void appendInspectorCatalogToken(QStringList &tokens, const QString &token)
{
    const QString normalized = token.trimmed().toLower();
    if (normalized.isEmpty() || normalized == QStringLiteral("*") || tokens.contains(normalized)) {
        return;
    }

    tokens.append(normalized);
}

QStringList sortedInspectorCatalogTokens(QStringList values)
{
    std::sort(values.begin(), values.end());
    return values;
}

QStringList defaultInspectorProjectionValues()
{
    return {
        QStringLiteral("plan"),
        QStringLiteral("extended"),
        QStringLiteral("elevation"),
        QStringLiteral("none"),
    };
}

void appendInspectorCommandMetadata(InspectorSymbolCatalog *catalog,
                                    const QString &commandName,
                                    const QJsonObject &commandObject)
{
    if (catalog == nullptr || commandName.isEmpty() || commandObject.isEmpty()) {
        return;
    }

    if (commandName == QStringLiteral("scrap")) {
        const QJsonArray options = commandObject.value(QStringLiteral("options")).toArray();
        for (const QJsonValue &optionValue : options) {
            const QJsonObject optionObject = optionValue.toObject();
            if (optionObject.value(QStringLiteral("option_key")).toString().trimmed().toLower()
                != QStringLiteral("-projection")) {
                continue;
            }
            const QJsonArray allowedValues = optionObject.value(QStringLiteral("allowed_values")).toArray();
            for (const QJsonValue &allowedValue : allowedValues) {
                appendInspectorCatalogToken(catalog->projectionValues, allowedValue.toString());
            }
        }
    }

    if (commandName != QStringLiteral("point")
        && commandName != QStringLiteral("line")
        && commandName != QStringLiteral("area")) {
        return;
    }

    if (commandName == QStringLiteral("point") && commandHasOption(commandObject, QStringLiteral("-value"))) {
        catalog->valueOptionTypesByCommand[commandName] = pointValueOptionTypes();
    }

    const QJsonArray typeValues = commandObject.value(QStringLiteral("type_values")).toArray();
    for (const QJsonValue &typeValue : typeValues) {
        appendInspectorCatalogToken(catalog->typeValuesByCommand[commandName], typeValue.toString());
    }

    const QJsonObject subtypeByType = commandObject.value(QStringLiteral("subtype_by_type")).toObject();
    for (auto subtypeIterator = subtypeByType.begin(); subtypeIterator != subtypeByType.end(); ++subtypeIterator) {
        const QString typeKey = subtypeIterator.key().trimmed().toLower();
        if (typeKey.isEmpty()) {
            continue;
        }

        const QJsonArray subtypeValues = subtypeIterator.value().toArray();
        for (const QJsonValue &subtypeValue : subtypeValues) {
            appendInspectorCatalogToken(catalog->subtypeValuesByCommandAndType[commandName][typeKey],
                                        subtypeValue.toString());
        }
    }
}

InspectorSymbolCatalog inspectorSymbolCatalogFromCommandCatalog(const QJsonObject &catalogObject)
{
    InspectorSymbolCatalog catalog;
    if (catalogObject.isEmpty()) {
        catalog.projectionValues = defaultInspectorProjectionValues();
        return catalog;
    }

    const QVector<InspectorCatalogCommandEntry> commands = inspectorCatalogCommandEntries(catalogObject);
    for (const InspectorCatalogCommandEntry &commandEntry : commands) {
        const QJsonObject commandObject = commandEntry.object;
        QString commandName = commandEntry.key.trimmed().toLower();
        if (commandName.isEmpty()) {
            commandName = commandObject.value(QStringLiteral("name")).toString().trimmed().toLower();
        }
        if (commandName.isEmpty()) {
            commandName = commandObject.value(QStringLiteral("directive")).toString().trimmed().toLower();
        }
        if (commandName.isEmpty()) {
            continue;
        }
        appendInspectorCommandMetadata(&catalog, commandName, commandObject);
    }

    if (catalog.projectionValues.isEmpty()) {
        catalog.projectionValues = defaultInspectorProjectionValues();
    }

    return catalog;
}

QStringList inspectorTypeValuesForCommand(const InspectorSymbolCatalog &catalog, const QString &commandKind)
{
    return sortedInspectorCatalogTokens(catalog.typeValuesByCommand.value(commandKind.trimmed().toLower()));
}

QStringList inspectorSubtypeValuesForCommandType(const InspectorSymbolCatalog &catalog,
                                                 const QString &commandKind,
                                                 const QString &type)
{
    return sortedInspectorCatalogTokens(catalog.subtypeValuesByCommandAndType.value(commandKind.trimmed().toLower()).value(type.trimmed().toLower()));
}

QStringList inspectorSubtypeValuesForCommandTypeWithEmptyChoice(const InspectorSymbolCatalog &catalog,
                                                                const QString &commandKind,
                                                                const QString &type)
{
    QStringList values = inspectorSubtypeValuesForCommandType(catalog, commandKind, type);
    if (!values.contains(QString())) {
        values.prepend(QString());
    }
    return values;
}

QStringList inspectorProjectionValues(const InspectorSymbolCatalog &catalog)
{
    return catalog.projectionValues;
}

bool inspectorValueOptionSupportedForCommandType(const InspectorSymbolCatalog &catalog,
                                                 const QString &commandKind,
                                                 const QString &type)
{
    const QSet<QString> supportedTypes = catalog.valueOptionTypesByCommand.value(commandKind.trimmed().toLower());
    return supportedTypes.contains(type.trimmed().toLower());
}

void setEditableComboValues(QComboBox *combo, const QStringList &values, const QString &currentText)
{
    if (combo == nullptr) {
        return;
    }

    const bool comboSignalsBlocked = combo->blockSignals(true);
    QLineEdit *lineEdit = combo->lineEdit();
    const bool lineEditSignalsBlocked = lineEdit != nullptr ? lineEdit->blockSignals(true) : false;
    combo->clear();
    combo->addItems(values);
    combo->setCurrentText(currentText);
    if (lineEdit != nullptr) {
        lineEdit->blockSignals(lineEditSignalsBlocked);
    }
    combo->blockSignals(comboSignalsBlocked);
}

std::optional<InspectorObjectQuickFields> inspectorObjectQuickFieldsFromParsedLine(const TherionParsedLine &parsedLine)
{
    if (parsedLine.directive == QStringLiteral("scrap")) {
        if (parsedLine.tokens.size() < 2) {
            return std::nullopt;
        }

        InspectorObjectQuickFields fields;
        fields.commandKind = parsedLine.directive;
        fields.identifier = parsedLine.tokens.at(1);
        fields.projection = inspectorBracketedOptionValue(parsedLine.tokens, QStringLiteral("-projection"));
        fields.typeEditable = false;
        return fields;
    }

    if (parsedLine.directive == QStringLiteral("point")) {
        const QString pointTypeToken = inspectorPointTypeToken(parsedLine);
        InspectorObjectQuickFields fields;
        fields.commandKind = parsedLine.directive;
        fields.type = inspectorTypePart(pointTypeToken);
        fields.subtype = inspectorObjectSubtype(parsedLine, pointTypeToken);
        const bool station = fields.type.compare(QStringLiteral("station"), Qt::CaseInsensitive) == 0;
        fields.identifier = commandOptionValue(parsedLine.tokens, QStringLiteral("-id"));
        fields.name = commandOptionValue(parsedLine.tokens, QStringLiteral("-name"));
        fields.text = commandOptionValue(parsedLine.tokens, QStringLiteral("-text"));
        fields.value = inspectorBracketedOptionValue(parsedLine.tokens, QStringLiteral("-value"));
        if (fields.name.isEmpty() && station) {
            fields.name = inspectorStationNameToken(parsedLine);
        }
        fields.nameVisible = station || !fields.name.isEmpty();
        fields.textVisible = inspectorObjectUsesTextField(fields.commandKind, fields.type, fields.text);
        fields.valueVisible = !fields.value.trimmed().isEmpty();
        return fields;
    }

    if (parsedLine.directive == QStringLiteral("line") || parsedLine.directive == QStringLiteral("area")) {
        if (parsedLine.tokens.size() < 2) {
            return std::nullopt;
        }

        const QString typeToken = parsedLine.tokens.at(1);
        InspectorObjectQuickFields fields;
        fields.commandKind = parsedLine.directive;
        fields.type = inspectorTypePart(typeToken);
        fields.subtype = inspectorObjectSubtype(parsedLine, typeToken);
        fields.identifier = commandOptionValue(parsedLine.tokens, QStringLiteral("-id"));
        fields.text = commandOptionValue(parsedLine.tokens, QStringLiteral("-text"));
        fields.textVisible = inspectorObjectUsesTextField(fields.commandKind, fields.type, fields.text);
        return fields;
    }

    return std::nullopt;
}

std::optional<InspectorObjectQuickFields> inspectorObjectQuickFieldsFromLogicalCommand(
    const TherionSourceLogicalCommand &command)
{
    return inspectorObjectQuickFieldsFromParsedLine(command.parsed);
}

}
