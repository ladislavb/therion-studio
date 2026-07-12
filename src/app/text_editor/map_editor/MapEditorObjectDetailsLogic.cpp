#include "MapEditorObjectDetailsLogic.h"

#include "../../../core/TherionCommandSyntax.h"
#include "../../../core/TherionSourceDocument.h"
#include "../../../core/TherionSourceText.h"
#include "../../../core/TherionTokenRules.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>
#include <QVector>

#include <cmath>

namespace TherionStudio
{
bool sourcePointsDifferForCommands(const QPointF &a, const QPointF &b)
{
    return (a - b).manhattanLength() > 0.01;
}

bool isConfigurableMapObjectKind(const QString &kind)
{
    const QString normalized = kind.trimmed().toLower();
    return normalized == QStringLiteral("scrap")
        || normalized == QStringLiteral("point")
        || normalized == QStringLiteral("line")
        || normalized == QStringLiteral("area");
}

QString objectKindForDirective(const QString &directiveToken)
{
    const QString directive = directiveToken.trimmed().toLower();
    if (directive == QStringLiteral("scrap")
        || directive == QStringLiteral("point")
        || directive == QStringLiteral("line")
        || directive == QStringLiteral("area")) {
        return directive;
    }
    return QString();
}

bool isOrientationOptionTokenForMapDetails(const QString &token)
{
    const QString normalized = token.trimmed().toLower();
    return normalized == QStringLiteral("-orientation")
        || normalized == QStringLiteral("orientation")
        || normalized == QStringLiteral("-orient")
        || normalized == QStringLiteral("orient");
}

qreal normalizeOrientationDegreesForMapDetails(qreal value)
{
    qreal normalized = std::fmod(value, 360.0);
    if (normalized < 0.0) {
        normalized += 360.0;
    }
    if (normalized >= 360.0) {
        normalized -= 360.0;
    }
    return normalized;
}

QString normalizeSymbolTypeTokenForMapDetails(const QString &token)
{
    QString normalized = token.trimmed().toLower();
    if (normalized.isEmpty() || normalized.startsWith(QLatin1Char('-'))) {
        return QString();
    }
    const int separatorIndex = normalized.indexOf(QLatin1Char(':'));
    if (separatorIndex > 0) {
        normalized = normalized.left(separatorIndex).trimmed();
    }
    return normalized;
}

QString lineTypeTokenForMapDetails(const TherionParsedLine &parsedLine)
{
    if (parsedLine.directive != QStringLiteral("line")) {
        return QString();
    }
    return normalizeSymbolTypeTokenForMapDetails(parsedLine.tokens.value(1));
}

QString pointTypeTokenForMapDetails(const TherionParsedLine &parsedLine)
{
    if (parsedLine.directive != QStringLiteral("point")) {
        return QString();
    }

    int numericCoordinateTokens = 0;
    for (int index = 1; index < parsedLine.tokens.size(); ++index) {
        const QString token = parsedLine.tokens.at(index).trimmed();
        if (token.isEmpty()) {
            continue;
        }
        if (TherionTokenRules::tokenStartsOption(token)) {
            break;
        }
        if (TherionTokenRules::isNumericToken(token)) {
            ++numericCoordinateTokens;
            continue;
        }
        if (numericCoordinateTokens < 2) {
            continue;
        }
        return normalizeSymbolTypeTokenForMapDetails(token);
    }

    return QString();
}

bool linePointRowDirectiveMatches(const TherionParsedLine &parsedLine, const QString &directive)
{
    const QString normalizedDirective = directive.trimmed().toLower();
    const QString parsedDirective = parsedLine.directive.trimmed().toLower();
    return parsedDirective == normalizedDirective
        || parsedDirective == QStringLiteral("-%1").arg(normalizedDirective);
}

bool linePointRowIsSegmentSubtype(const QString &row)
{
    const TherionSourceDocument sourceDocument = TherionSourceDocument::fromText(row);
    const TherionSourceDocumentLine *sourceLine = sourceDocument.lineAtLineNumber(1);
    return sourceLine != nullptr
        && linePointRowDirectiveMatches(sourceLine->sourceLine.parsed, QStringLiteral("subtype"));
}

bool linePointRowIsAltitudeAuto(const QString &row)
{
    const TherionSourceDocument sourceDocument = TherionSourceDocument::fromText(row);
    const TherionSourceDocumentLine *sourceLine = sourceDocument.lineAtLineNumber(1);
    return sourceLine != nullptr
        && linePointRowDirectiveMatches(sourceLine->sourceLine.parsed, QStringLiteral("altitude"))
        && sourceLine->sourceLine.parsed.tokens.value(1).trimmed() == QStringLiteral(".");
}

QSet<QString> parseOrientationAllowedTypesFromText(const QString &text)
{
    QSet<QString> allowedTypes;
    if (text.trimmed().isEmpty()) {
        return allowedTypes;
    }

    static const QRegularExpression onlyWithTypePattern(
        QStringLiteral(R"(only[^.\n\r]{0,200}?\b([a-z][a-z0-9_-]*)\s+type\b)"),
        QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatchIterator onlyWithMatches = onlyWithTypePattern.globalMatch(text);
    while (onlyWithMatches.hasNext()) {
        const QRegularExpressionMatch match = onlyWithMatches.next();
        const QString typeToken = normalizeSymbolTypeTokenForMapDetails(match.captured(1));
        if (!typeToken.isEmpty()) {
            allowedTypes.insert(typeToken);
        }
    }

    static const QRegularExpression quotedSymbolTypePattern(
        QStringLiteral(R"((?:`|'|\|)([a-z][a-z0-9_-]*)(?:`|'|\|)\s+symbol\s+type\b)"),
        QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatchIterator quotedMatches = quotedSymbolTypePattern.globalMatch(text);
    while (quotedMatches.hasNext()) {
        const QRegularExpressionMatch match = quotedMatches.next();
        const QString typeToken = normalizeSymbolTypeTokenForMapDetails(match.captured(1));
        if (!typeToken.isEmpty()) {
            allowedTypes.insert(typeToken);
        }
    }

    return allowedTypes;
}

struct ObjectDetailsCatalogCommandEntry
{
    QString key;
    QJsonObject object;
};

QSet<QString> stringSetFromJsonArray(const QJsonArray &values)
{
    QSet<QString> tokens;
    for (const QJsonValue &value : values) {
        const QString token = normalizeSymbolTypeTokenForMapDetails(value.toString());
        if (!token.isEmpty()) {
            tokens.insert(token);
        }
    }
    return tokens;
}

QVector<ObjectDetailsCatalogCommandEntry> objectDetailsCatalogCommandEntries(const QJsonObject &catalogObject)
{
    QVector<ObjectDetailsCatalogCommandEntry> entries;
    const QJsonValue commandsValue = catalogObject.value(QStringLiteral("commands"));
    if (commandsValue.isArray()) {
        const QJsonArray commandsArray = commandsValue.toArray();
        entries.reserve(commandsArray.size());
        for (const QJsonValue &commandValue : commandsArray) {
            const QJsonObject commandObject = commandValue.toObject();
            if (commandObject.isEmpty()) {
                continue;
            }

            ObjectDetailsCatalogCommandEntry entry;
            entry.key = commandObject.value(QStringLiteral("name")).toString();
            if (entry.key.trimmed().isEmpty()) {
                entry.key = commandObject.value(QStringLiteral("directive")).toString();
            }
            entry.object = commandObject;
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

            ObjectDetailsCatalogCommandEntry entry;
            entry.key = it.key();
            entry.object = it.value().toObject();
            entries.append(entry);
        }
    }

    return entries;
}

MapEditorOrientationApplicabilityByCommand mapEditorOrientationApplicabilityFromCommandCatalog(const QJsonObject &catalogObject)
{
    MapEditorOrientationApplicabilityByCommand applicabilityByCommand;
    if (catalogObject.isEmpty()) {
        return applicabilityByCommand;
    }

    const QVector<ObjectDetailsCatalogCommandEntry> commands = objectDetailsCatalogCommandEntries(catalogObject);
    for (const ObjectDetailsCatalogCommandEntry &commandEntry : commands) {
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

        const QJsonArray options = commandObject.value(QStringLiteral("options")).toArray();
        for (const QJsonValue &optionValue : options) {
            const QJsonObject optionObject = optionValue.toObject();
            const QString optionKey = optionObject.value(QStringLiteral("option_key")).toString().trimmed().toLower();
            if (optionKey != QStringLiteral("-orientation")) {
                continue;
            }

            QStringList metadataTexts;
            metadataTexts.append(optionObject.value(QStringLiteral("description")).toString());
            metadataTexts.append(optionObject.value(QStringLiteral("raw")).toString());
            metadataTexts.append(optionObject.value(QStringLiteral("name")).toString());
            metadataTexts.append(optionObject.value(QStringLiteral("signature")).toString());
            const QJsonArray dependencies = optionObject.value(QStringLiteral("dependencies")).toArray();
            for (const QJsonValue &dependencyValue : dependencies) {
                metadataTexts.append(dependencyValue.toString());
            }

            MapEditorOrientationApplicability applicability;
            applicability.allowedTypes.unite(stringSetFromJsonArray(optionObject.value(QStringLiteral("applicable_types")).toArray()));
            applicability.excludedTypes.unite(stringSetFromJsonArray(optionObject.value(QStringLiteral("excluded_types")).toArray()));
            for (const QString &metadataText : metadataTexts) {
                applicability.allowedTypes.unite(parseOrientationAllowedTypesFromText(metadataText));
            }

            applicabilityByCommand.insert(commandName, applicability);
            break;
        }
    }

    return applicabilityByCommand;
}

bool isOrientationSupportedForParsedLine(const TherionParsedLine &parsedLine,
                                         const MapEditorOrientationApplicabilityByCommand &applicabilityByCommand)
{
    const QString commandName = parsedLine.directive.trimmed().toLower();
    if (commandName != QStringLiteral("point") && commandName != QStringLiteral("line")) {
        return false;
    }

    if (!applicabilityByCommand.contains(commandName)) {
        return true;
    }

    const MapEditorOrientationApplicability applicability = applicabilityByCommand.value(commandName);
    if (applicability.allowedTypes.isEmpty() && applicability.excludedTypes.isEmpty()) {
        return true;
    }

    const QString symbolType = commandName == QStringLiteral("point")
        ? pointTypeTokenForMapDetails(parsedLine)
        : lineTypeTokenForMapDetails(parsedLine);
    if (symbolType.isEmpty()) {
        return false;
    }

    if (applicability.excludedTypes.contains(symbolType)) {
        return false;
    }
    if (!applicability.allowedTypes.isEmpty()) {
        return applicability.allowedTypes.contains(symbolType);
    }
    return true;
}

bool isLinePointLeftSizeSupportedForParsedLine(const TherionParsedLine &parsedLine)
{
    return parsedLine.directive == QStringLiteral("line")
        && lineTypeTokenForMapDetails(parsedLine) == QStringLiteral("slope");
}

QVector<QPair<int, int>> coordinateTokenPairsForLine(const TherionParsedLine &parsedLine, int startTokenIndex)
{
    QVector<QPair<int, int>> pairs;
    QVector<int> numericIndices;
    numericIndices.reserve(parsedLine.tokens.size());

    const int firstTokenIndex = qMax(0, startTokenIndex);
    if (firstTokenIndex == 0) {
        int firstNonQuotedIndex = -1;
        for (int index = firstTokenIndex; index < parsedLine.tokens.size(); ++index) {
            if (index < parsedLine.tokenSpans.size()
                && parsedLine.tokenSpans.at(index).type == TherionTokenType::QuotedString) {
                continue;
            }
            firstNonQuotedIndex = index;
            break;
        }

        if (firstNonQuotedIndex >= 0 && !TherionTokenRules::isNumericToken(parsedLine.tokens.at(firstNonQuotedIndex))) {
            return pairs;
        }
    }

    if (firstTokenIndex < parsedLine.tokens.size()) {
        const QString firstToken = parsedLine.tokens.at(firstTokenIndex);
        if (firstTokenIndex == 0
            && firstToken.startsWith(QLatin1Char('-'))
            && !TherionTokenRules::isNumericToken(firstToken)) {
            return pairs;
        }
    }

    bool sawCoordinateToken = false;
    bool skipOptionValueToken = false;
    for (int index = firstTokenIndex; index < parsedLine.tokens.size(); ++index) {
        if (skipOptionValueToken && !sawCoordinateToken) {
            skipOptionValueToken = false;
            continue;
        }

        const QString token = parsedLine.tokens.at(index);
        if (!sawCoordinateToken
            && firstTokenIndex > 0
            && token.startsWith(QLatin1Char('-'))
            && !TherionTokenRules::isNumericToken(token)) {
            if (index + 1 < parsedLine.tokens.size()) {
                const QString nextToken = parsedLine.tokens.at(index + 1);
                if (!TherionTokenRules::tokenStartsOption(nextToken)) {
                    skipOptionValueToken = true;
                }
            }
            continue;
        }

        const bool numeric = TherionTokenRules::isNumericToken(token);
        if (!numeric) {
            if (sawCoordinateToken) {
                break;
            }
            continue;
        }

        if (index < parsedLine.tokenSpans.size()
            && parsedLine.tokenSpans.at(index).type == TherionTokenType::QuotedString) {
            continue;
        }

        numericIndices.append(index);
        sawCoordinateToken = true;
    }

    for (int index = 0; index + 1 < numericIndices.size(); index += 2) {
        pairs.append(qMakePair(numericIndices.at(index), numericIndices.at(index + 1)));
    }
    return pairs;
}

std::optional<qreal> pointOrientationFromParsedLine(const TherionParsedLine &parsedLine)
{
    std::optional<qreal> orientation;
    for (int index = 1; index < parsedLine.tokens.size(); ++index) {
        if (!isOrientationOptionTokenForMapDetails(parsedLine.tokens.at(index))) {
            continue;
        }
        if (index + 1 >= parsedLine.tokens.size()) {
            continue;
        }
        const QString valueToken = parsedLine.tokens.at(index + 1).trimmed();
        if (TherionTokenRules::tokenStartsOption(valueToken)) {
            continue;
        }
        bool ok = false;
        const qreal value = valueToken.toDouble(&ok);
        if (!ok) {
            continue;
        }
        orientation = normalizeOrientationDegreesForMapDetails(value);
    }
    return orientation;
}

bool isLinePointOptionTokenForMapDetails(const QString &token, const QStringList &optionNames)
{
    const QString normalized = token.trimmed().toLower();
    for (const QString &optionName : optionNames) {
        if (normalized == optionName.trimmed().toLower()) {
            return true;
        }
    }
    return false;
}

const TherionParsedLine *parsedSourceLineAt(const TherionSourceDocument &sourceDocument, int lineNumber)
{
    const TherionSourceDocumentLine *sourceLine = sourceDocument.lineAtLineNumber(lineNumber);
    return sourceLine != nullptr ? &sourceLine->sourceLine.parsed : nullptr;
}

std::optional<qreal> linePointNumericOptionForSourceVertex(const QString &documentText,
                                                           int lineNumber,
                                                           int sourceVertexIndex,
                                                           const QStringList &optionNames,
                                                           bool normalizeOrientation)
{
    if (lineNumber <= 0 || sourceVertexIndex < 0) {
        return std::nullopt;
    }

    const TherionSourceDocument sourceDocument = TherionSourceDocument::fromText(documentText);
    const int lineCount = sourceDocument.parsedDocument().lines.size();
    if (lineNumber > lineCount) {
        return std::nullopt;
    }

    const int blockStartLineIndex = lineNumber - 1;
    const TherionParsedLine *startLine = parsedSourceLineAt(sourceDocument, lineNumber);
    if (startLine == nullptr || startLine->directive != QStringLiteral("line")) {
        return std::nullopt;
    }

    int blockEndLineIndex = -1;
    for (int candidateIndex = blockStartLineIndex + 1; candidateIndex < lineCount; ++candidateIndex) {
        const TherionParsedLine *candidateLine = parsedSourceLineAt(sourceDocument, candidateIndex + 1);
        if (candidateLine != nullptr && candidateLine->directive == QStringLiteral("endline")) {
            blockEndLineIndex = candidateIndex;
            break;
        }
    }
    if (blockEndLineIndex < 0) {
        return std::nullopt;
    }

    int nextSourceIndex = 0;
    for (int rowIndex = blockStartLineIndex; rowIndex < blockEndLineIndex; ++rowIndex) {
        const TherionParsedLine *parsedLine = parsedSourceLineAt(sourceDocument, rowIndex + 1);
        if (parsedLine == nullptr) {
            continue;
        }
        const int startTokenIndex = rowIndex == blockStartLineIndex ? 1 : 0;
        const QVector<QPair<int, int>> coordinatePairs = coordinateTokenPairsForLine(*parsedLine, startTokenIndex);
        if (coordinatePairs.isEmpty()) {
            continue;
        }
        const int localPairCount = coordinatePairs.size();

        const int firstSourceIndex = nextSourceIndex;
        nextSourceIndex += localPairCount;
        if (sourceVertexIndex < firstSourceIndex || sourceVertexIndex >= nextSourceIndex) {
            continue;
        }

        auto optionValueFromLine = [&](const TherionParsedLine &optionLine, int firstTokenIndex) -> std::optional<qreal> {
            std::optional<qreal> optionValue;
            for (int optionIndex = firstTokenIndex; optionIndex < optionLine.tokens.size(); ++optionIndex) {
                if (!isLinePointOptionTokenForMapDetails(optionLine.tokens.at(optionIndex), optionNames)) {
                    continue;
                }
                if (optionIndex + 1 >= optionLine.tokens.size()) {
                    continue;
                }
                const QString valueToken = optionLine.tokens.at(optionIndex + 1).trimmed();
                if (TherionTokenRules::tokenStartsOption(valueToken)) {
                    continue;
                }
                bool ok = false;
                const qreal value = valueToken.toDouble(&ok);
                if (ok) {
                    optionValue = normalizeOrientation ? normalizeOrientationDegreesForMapDetails(value) : value;
                }
            }
            return optionValue;
        };

        std::optional<qreal> optionValue =
            optionValueFromLine(*parsedLine, qMax(startTokenIndex, coordinatePairs.at(sourceVertexIndex - firstSourceIndex).second + 1));
        for (int optionRowIndex = rowIndex + 1; optionRowIndex < blockEndLineIndex; ++optionRowIndex) {
            const TherionParsedLine *optionRowLine = parsedSourceLineAt(sourceDocument, optionRowIndex + 1);
            if (optionRowLine == nullptr) {
                continue;
            }
            if (!coordinateTokenPairsForLine(*optionRowLine, 0).isEmpty()) {
                break;
            }
            if (const std::optional<qreal> rowValue = optionValueFromLine(*optionRowLine, 0)) {
                optionValue = rowValue;
            }
        }
        return optionValue;
    }

    return std::nullopt;
}

TherionSourceDocument linePointStandaloneRowsDocument(const QStringList &rows)
{
    return TherionSourceDocument::fromText(rows.join(QLatin1Char('\n')));
}

std::optional<qreal> linePointOrientationForSourceVertex(const QString &documentText,
                                                         int lineNumber,
                                                         int sourceVertexIndex)
{
    return linePointNumericOptionForSourceVertex(documentText,
                                                lineNumber,
                                                sourceVertexIndex,
                                                QStringList{QStringLiteral("-orientation"),
                                                            QStringLiteral("orientation"),
                                                            QStringLiteral("-orient"),
                                                            QStringLiteral("orient")},
                                                true);
}

std::optional<qreal> linePointLeftSizeForSourceVertex(const QString &documentText,
                                                      int lineNumber,
                                                      int sourceVertexIndex)
{
    std::optional<qreal> size = linePointNumericOptionForSourceVertex(documentText,
                                                                     lineNumber,
                                                                     sourceVertexIndex,
                                                                     QStringList{QStringLiteral("-size"),
                                                                                 QStringLiteral("size"),
                                                                                 QStringLiteral("-l-size"),
                                                                                 QStringLiteral("l-size")},
                                                                     false);
    if (size.has_value() && size.value() > 0.0) {
        return size;
    }
    return std::nullopt;
}

QString linePointSegmentSubtypeFromStandaloneRows(const QStringList &rows)
{
    const TherionSourceDocument sourceDocument = linePointStandaloneRowsDocument(rows);
    QString subtype;
    for (int rowIndex = 0; rowIndex < rows.size(); ++rowIndex) {
        const TherionSourceDocumentLine *sourceLine = sourceDocument.lineAtLineNumber(rowIndex + 1);
        if (sourceLine == nullptr) {
            continue;
        }
        const TherionParsedLine &parsedLine = sourceLine->sourceLine.parsed;
        if (linePointRowDirectiveMatches(parsedLine, QStringLiteral("subtype"))
            && parsedLine.tokens.size() > 1) {
            subtype = parsedLine.tokens.value(1).trimmed();
        }
    }
    return subtype;
}

bool linePointAltitudeAutoFromStandaloneRows(const QStringList &rows)
{
    const TherionSourceDocument sourceDocument = linePointStandaloneRowsDocument(rows);
    for (int rowIndex = 0; rowIndex < rows.size(); ++rowIndex) {
        const TherionSourceDocumentLine *sourceLine = sourceDocument.lineAtLineNumber(rowIndex + 1);
        if (sourceLine == nullptr) {
            continue;
        }
        if (linePointRowDirectiveMatches(sourceLine->sourceLine.parsed, QStringLiteral("altitude"))
            && sourceLine->sourceLine.parsed.tokens.value(1).trimmed() == QStringLiteral(".")) {
            return true;
        }
    }
    return false;
}

bool linePointRowsShouldHighlightVertexMetadata(const QStringList &rows)
{
    const TherionSourceDocument sourceDocument = linePointStandaloneRowsDocument(rows);
    for (int rowIndex = 0; rowIndex < rows.size(); ++rowIndex) {
        const TherionSourceDocumentLine *sourceLine = sourceDocument.lineAtLineNumber(rowIndex + 1);
        if (sourceLine == nullptr) {
            continue;
        }
        const TherionParsedLine &parsedLine = sourceLine->sourceLine.parsed;
        if (linePointRowDirectiveMatches(parsedLine, QStringLiteral("altitude"))
            || linePointRowDirectiveMatches(parsedLine, QStringLiteral("subtype"))) {
            return true;
        }
    }
    return false;
}

QStringList linePointRowsWithoutStructuredStandaloneOptions(const QStringList &rows,
                                                            bool manageSegmentSubtype,
                                                            bool manageAltitudeAuto)
{
    const TherionSourceDocument sourceDocument = linePointStandaloneRowsDocument(rows);
    QStringList filteredRows;
    filteredRows.reserve(rows.size());
    for (int rowIndex = 0; rowIndex < rows.size(); ++rowIndex) {
        const TherionSourceDocumentLine *sourceLine = sourceDocument.lineAtLineNumber(rowIndex + 1);
        const TherionParsedLine parsedLine = sourceLine != nullptr
            ? sourceLine->sourceLine.parsed
            : TherionParsedLine{};
        if ((manageSegmentSubtype && linePointRowDirectiveMatches(parsedLine, QStringLiteral("subtype"))
             && parsedLine.tokens.size() > 1)
            || (manageAltitudeAuto
                && linePointRowDirectiveMatches(parsedLine, QStringLiteral("altitude"))
                && parsedLine.tokens.value(1).trimmed() == QStringLiteral("."))) {
            continue;
        }
        filteredRows.append(rows.at(rowIndex));
    }
    return filteredRows;
}

QStringList linePointRowsWithStructuredStandaloneOptions(const QStringList &rows,
                                                         const QString &segmentSubtype,
                                                         bool altitudeAuto,
                                                         bool manageSegmentSubtype,
                                                         bool manageAltitudeAuto)
{
    QStringList updatedRows = linePointRowsWithoutStructuredStandaloneOptions(rows,
                                                                              manageSegmentSubtype,
                                                                              manageAltitudeAuto);
    const QString trimmedSubtype = segmentSubtype.trimmed();
    if (manageSegmentSubtype && !trimmedSubtype.isEmpty()) {
        const QString serializedSubtype = serializeTherionArgumentToken(trimmedSubtype);
        if (!serializedSubtype.isEmpty()) {
            updatedRows.append(QStringLiteral("subtype %1").arg(serializedSubtype));
        }
    }
    if (manageAltitudeAuto && altitudeAuto) {
        updatedRows.append(QStringLiteral("altitude ."));
    }
    return updatedRows;
}

}
