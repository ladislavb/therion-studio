#include "TherionDocumentEditor.h"

#include "TherionCommandSyntax.h"
#include "TherionDocumentParser.h"
#include "TherionSourceDocument.h"
#include "TherionStringUtils.h"
#include "TherionTokenRules.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QPair>
#include <QSet>
#include <algorithm>
#include <cmath>
#include <optional>
#include <utility>

namespace TherionStudio
{
namespace
{
int optionValueTokenIndex(const TherionParsedLine &parsedLine, const QString &option)
{
    const QString normalizedOption = option.toLower();
    for (int index = 0; index + 1 < parsedLine.tokens.size(); ++index) {
        if (parsedLine.tokens.at(index).toLower() != normalizedOption) {
            continue;
        }

        const QString candidate = parsedLine.tokens.at(index + 1);
        if (!candidate.startsWith(QLatin1Char('-'))) {
            return index + 1;
        }
    }

    return -1;
}

int pointTypeTokenIndex(const TherionParsedLine &parsedLine)
{
    bool skipOptionValue = false;
    for (int index = 1; index < parsedLine.tokens.size(); ++index) {
        const QString token = parsedLine.tokens.at(index);
        if (skipOptionValue) {
            skipOptionValue = false;
            continue;
        }
        if (TherionTokenRules::isNumericToken(token)) {
            continue;
        }
        if (token.startsWith(QLatin1Char('-'))) {
            skipOptionValue = index + 1 < parsedLine.tokens.size();
            continue;
        }

        return index;
    }

    return -1;
}

int nameTokenIndexForCategory(const QString &category, const TherionParsedLine &parsedLine)
{
    const QString normalizedCategory = category.trimmed().toLower();
    const QString directive = parsedLine.directive;

    if (normalizedCategory == QStringLiteral("stations")) {
        if (directive == QStringLiteral("point")) {
            const int stationNameIndex = optionValueTokenIndex(parsedLine, QStringLiteral("-name"));
            if (stationNameIndex >= 0) {
                return stationNameIndex;
            }

            const int pointTypeIndex = pointTypeTokenIndex(parsedLine);
            if (pointTypeIndex >= 0 && parsedLine.tokens.value(pointTypeIndex).toLower() == QStringLiteral("station")) {
                const int stationIdentifierIndex = pointTypeIndex + 1;
                if (stationIdentifierIndex < parsedLine.tokens.size()) {
                    const QString token = parsedLine.tokens.at(stationIdentifierIndex);
                    if (!token.startsWith(QLatin1Char('-'))) {
                        return stationIdentifierIndex;
                    }
                }

                return pointTypeIndex;
            }
        }

        if (directive == QStringLiteral("station") && parsedLine.tokens.size() >= 2) {
            return 1;
        }
    }

    if (normalizedCategory == QStringLiteral("lines") || normalizedCategory == QStringLiteral("areas")) {
        const int identifierIndex = optionValueTokenIndex(parsedLine, QStringLiteral("-id"));
        if (identifierIndex >= 0) {
            return identifierIndex;
        }

        return parsedLine.tokens.size() >= 2 ? 1 : -1;
    }

    if (normalizedCategory == QStringLiteral("points")) {
        const int labelIndex = optionValueTokenIndex(parsedLine, QStringLiteral("-name"));
        if (labelIndex >= 0) {
            return labelIndex;
        }

        const int identifierIndex = optionValueTokenIndex(parsedLine, QStringLiteral("-id"));
        if (identifierIndex >= 0) {
            return identifierIndex;
        }

        const int pointTypeIndex = pointTypeTokenIndex(parsedLine);
        if (pointTypeIndex >= 0) {
            return pointTypeIndex;
        }
    }

    if (normalizedCategory == QStringLiteral("surveys")
        || normalizedCategory == QStringLiteral("maps")
        || normalizedCategory == QStringLiteral("scraps")) {
        return 1;
    }

    return -1;
}

QString quoteToken(const QString &token, QChar quoteCharacter)
{
    QString quotedToken = token;
    quotedToken.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    quotedToken.replace(quoteCharacter, QStringLiteral("\\") + quoteCharacter);
    return QString(quoteCharacter) + quotedToken + QString(quoteCharacter);
}

QString replacementTokenForLine(const QString &newName, const QString &lineText, const TherionParsedToken &tokenSpan)
{
    const QString trimmedName = newName.trimmed();
    const bool requiresQuoting = trimmedName.isEmpty()
        || trimmedName.contains(QRegularExpression(QStringLiteral(R"([\s#%\\'\"])")));

    if (tokenSpan.type != TherionTokenType::QuotedString && !requiresQuoting) {
        return trimmedName;
    }

    const QChar quoteCharacter = (tokenSpan.start < lineText.size() && lineText.at(tokenSpan.start) == QLatin1Char('\''))
        ? QLatin1Char('\'')
        : QLatin1Char('"');
    return quoteToken(trimmedName, quoteCharacter);
}

bool insertPhysicalSourceLines(QString *contents,
                               int insertionLineIndex,
                               const QStringList &insertedLines,
                               QString *errorMessage)
{
    if (contents == nullptr) {
        return false;
    }
    if (insertedLines.isEmpty()) {
        return true;
    }

    const TherionParsedSourceDocument sourceDocument = TherionDocumentParser::parseSourceDocument(*contents);
    if (insertionLineIndex < 0 || insertionLineIndex > sourceDocument.lines.size()) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "Insertion source range could not be resolved.");
        }
        return false;
    }

    QString lineEnding;
    if (insertionLineIndex > 0 && insertionLineIndex - 1 < sourceDocument.lines.size()) {
        lineEnding = sourceDocument.lines.at(insertionLineIndex - 1).lineEnding;
    }
    if (lineEnding.isEmpty() && insertionLineIndex < sourceDocument.lines.size()) {
        lineEnding = sourceDocument.lines.at(insertionLineIndex).lineEnding;
    }
    if (lineEnding.isEmpty()) {
        lineEnding = TherionSourceText::detectedLineEnding(*contents);
    }

    QString insertedText;
    for (const QString &line : insertedLines) {
        insertedText += line;
        insertedText += lineEnding;
    }

    const int insertOffset = insertionLineIndex < sourceDocument.lines.size()
        ? sourceDocument.lines.at(insertionLineIndex).startOffset
        : contents->size();
    contents->insert(insertOffset, insertedText);
    return true;
}

bool physicalSourceLineInsertionEdit(const QString &contents,
                                     int insertionLineIndex,
                                     const QStringList &insertedLines,
                                     TherionSourceTextEdit *edit,
                                     QString *errorMessage)
{
    if (edit == nullptr) {
        return false;
    }
    if (insertedLines.isEmpty()) {
        *edit = TherionSourceTextEdit{};
        return true;
    }

    const TherionParsedSourceDocument sourceDocument = TherionDocumentParser::parseSourceDocument(contents);
    if (insertionLineIndex < 0 || insertionLineIndex > sourceDocument.lines.size()) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "Insertion source range could not be resolved.");
        }
        return false;
    }

    QString lineEnding;
    if (insertionLineIndex > 0 && insertionLineIndex - 1 < sourceDocument.lines.size()) {
        lineEnding = sourceDocument.lines.at(insertionLineIndex - 1).lineEnding;
    }
    if (lineEnding.isEmpty() && insertionLineIndex < sourceDocument.lines.size()) {
        lineEnding = sourceDocument.lines.at(insertionLineIndex).lineEnding;
    }
    if (lineEnding.isEmpty()) {
        lineEnding = TherionSourceText::detectedLineEnding(contents);
    }

    QString insertedText;
    for (const QString &line : insertedLines) {
        insertedText += line;
        insertedText += lineEnding;
    }

    const int insertOffset = insertionLineIndex < sourceDocument.lines.size()
        ? sourceDocument.lines.at(insertionLineIndex).startOffset
        : static_cast<int>(contents.size());
    *edit = TherionSourceTextEdit{
        insertOffset,
        0,
        insertedText,
    };
    return true;
}

QString serializedInlineToken(const QString &value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty()) {
        return QString();
    }

    if (!trimmed.contains(QRegularExpression(QStringLiteral(R"([\s#%\\'\"])")))) {
        return trimmed;
    }

    return quoteToken(trimmed, QLatin1Char('"'));
}

QString sanitizeScrapName(const QString &preferredName)
{
    QString source = preferredName.trimmed().toLower();
    if (source.isEmpty()) {
        source = QStringLiteral("new-scrap");
    }

    QString normalized;
    normalized.reserve(source.size());
    bool previousDash = false;
    for (const QChar character : source) {
        const bool allowed = character.isLetterOrNumber()
            || character == QLatin1Char('-')
            || character == QLatin1Char('_')
            || character == QLatin1Char('.');

        if (allowed) {
            normalized.append(character);
            previousDash = false;
            continue;
        }

        if (!previousDash) {
            normalized.append(QLatin1Char('-'));
            previousDash = true;
        }
    }

    while (normalized.startsWith(QLatin1Char('-'))) {
        normalized.remove(0, 1);
    }
    while (normalized.endsWith(QLatin1Char('-'))) {
        normalized.chop(1);
    }

    if (normalized.isEmpty()) {
        return QStringLiteral("new-scrap");
    }

    return normalized;
}

void collectIdentifiersFromTokens(const QStringList &tokens, QSet<QString> *identifiers)
{
    if (identifiers == nullptr) {
        return;
    }

    for (int index = 0; index + 1 < tokens.size(); ++index) {
        if (tokens.at(index).toLower() != QStringLiteral("-id")) {
            continue;
        }

        const QString candidate = tokens.at(index + 1).trimmed();
        if (candidate.isEmpty() || candidate.startsWith(QLatin1Char('-'))) {
            continue;
        }
        identifiers->insert(candidate.toLower());
    }
}

QString generatedIdentifier(const QString &prefix, const QSet<QString> &existingIdentifiers)
{
    QString normalizedPrefix = prefix.trimmed().toLower();
    if (normalizedPrefix.isEmpty()) {
        normalizedPrefix = QStringLiteral("object");
    }
    int suffix = 1;
    while (existingIdentifiers.contains(QStringLiteral("%1-%2").arg(normalizedPrefix).arg(suffix))) {
        ++suffix;
    }
    return QStringLiteral("%1-%2").arg(normalizedPrefix).arg(suffix);
}

int matchingScrapStartIndex(const QStringList &lines, int endscrapIndex)
{
    if (endscrapIndex < 0 || endscrapIndex >= lines.size()) {
        return -1;
    }

    int depth = 0;
    for (int index = endscrapIndex; index >= 0; --index) {
        const TherionParsedLine parsedLine = TherionDocumentParser::parseLine(lines.at(index), index + 1);
        if (parsedLine.directive == QStringLiteral("endscrap")) {
            ++depth;
            continue;
        }
        if (parsedLine.directive != QStringLiteral("scrap")) {
            continue;
        }
        --depth;
        if (depth == 0) {
            return index;
        }
    }

    return -1;
}

QSet<QString> identifiersInsideScrap(const QStringList &lines, int scrapStartIndex, int endscrapIndex)
{
    QSet<QString> identifiers;
    if (scrapStartIndex < 0 || endscrapIndex <= scrapStartIndex || endscrapIndex > lines.size()) {
        return identifiers;
    }

    for (int index = scrapStartIndex + 1; index < endscrapIndex; ++index) {
        const TherionParsedLine parsedLine = TherionDocumentParser::parseLine(lines.at(index), index + 1);
        collectIdentifiersFromTokens(parsedLine.tokens, &identifiers);
    }

    return identifiers;
}

int lineCountForText(const QString &text)
{
    if (text.isEmpty()) {
        return 0;
    }

    int lineCount = text.count(QLatin1Char('\n'));
    if (!text.endsWith(QLatin1Char('\n'))) {
        ++lineCount;
    }

    return qMax(1, lineCount);
}

QString formatCoordinate(qreal value)
{
    return QString::number(value, 'f', 1);
}

QString draftObjectTypeToken(const TherionDraftObjectOptions &options, const QString &fallbackType)
{
    const QString type = options.type.trimmed().isEmpty()
        ? fallbackType.trimmed()
        : options.type.trimmed();
    const QString serializedType = serializedInlineToken(type);
    return serializedType.isEmpty() ? serializedInlineToken(fallbackType) : serializedType;
}

QString draftObjectOptionsSuffix(const TherionDraftObjectOptions &options, bool includeName)
{
    QString suffix;
    const QString normalizedSubtype = options.subtype.trimmed();
    const QString subtype = TherionTokenRules::tokenStartsOption(normalizedSubtype)
        ? QString()
        : serializedInlineToken(normalizedSubtype);
    if (!subtype.isEmpty()) {
        suffix += QStringLiteral(" -subtype %1").arg(subtype);
    }

    const QString identifier = serializedInlineToken(options.identifier);
    if (!identifier.isEmpty()) {
        suffix += QStringLiteral(" -id %1").arg(identifier);
    }

    const QString name = serializedInlineToken(options.name);
    if (includeName && options.nameEnabled && !name.isEmpty()) {
        suffix += QStringLiteral(" -name %1").arg(name);
    }

    const QString text = serializedInlineToken(options.text);
    if (options.textEnabled && !text.isEmpty()) {
        suffix += QStringLiteral(" -text %1").arg(text);
    }

    const QString trimmedValue = options.value.trimmed();
    const QString value = trimmedValue.isEmpty()
        ? QString()
        : serializeTherionArgumentToken(trimmedValue);
    if (options.valueEnabled && !value.isEmpty()) {
        suffix += QStringLiteral(" -value %1").arg(value);
    }
    return suffix;
}

bool pointTypeSupportsValueOption(const QString &type)
{
    const QString normalized = type.trimmed().toLower();
    return normalized == QStringLiteral("altitude")
        || normalized == QStringLiteral("date")
        || normalized == QStringLiteral("dimensions")
        || normalized == QStringLiteral("height")
        || normalized == QStringLiteral("passage-height");
}

QString draftObjectHeader(const QString &directive,
                          const QString &fallbackType,
                          const TherionDraftObjectOptions &options,
                          bool includeName = false)
{
    return QStringLiteral("%1 %2%3")
        .arg(directive,
             draftObjectTypeToken(options, fallbackType),
             draftObjectOptionsSuffix(options, includeName));
}

QString formatCoordinateLikeExistingToken(const QString &existingToken, qreal value)
{
    QString token = existingToken.trimmed();
    if (token.isEmpty()) {
        return formatCoordinate(value);
    }

    const int exponentIndex = token.indexOf(QRegularExpression(QStringLiteral("[eE]")));
    if (exponentIndex >= 0) {
        token = token.left(exponentIndex);
    }

    int decimalPlaces = 0;
    const int dotIndex = token.indexOf(QLatin1Char('.'));
    if (dotIndex >= 0) {
        int index = dotIndex + 1;
        while (index < token.size() && token.at(index).isDigit()) {
            ++decimalPlaces;
            ++index;
        }
    }

    if (decimalPlaces == 0) {
        const qreal nearestInteger = std::round(value);
        if (std::fabs(value - nearestInteger) > 1e-6) {
            decimalPlaces = 3;
        }
    }

    return QString::number(value, 'f', decimalPlaces);
}

std::optional<QString> normalizedLineToggleOptionName(const QString &optionName)
{
    const QString normalized = optionName.trimmed().toLower();
    if (normalized == QStringLiteral("-close") || normalized == QStringLiteral("close")) {
        return QStringLiteral("-close");
    }
    if (normalized == QStringLiteral("-reverse") || normalized == QStringLiteral("reverse")) {
        return QStringLiteral("-reverse");
    }

    return std::nullopt;
}

bool isOrientationOptionToken(const QString &token)
{
    const QString normalized = token.trimmed().toLower();
    return normalized == QStringLiteral("-orientation") || normalized == QStringLiteral("-orient");
}

bool isLinePointOptionToken(const QString &token, const QString &canonicalOption)
{
    const QString normalized = token.trimmed().toLower();
    const QString canonical = canonicalOption.trimmed().toLower();
    if (canonical == QStringLiteral("-orientation")) {
        return normalized == QStringLiteral("-orientation")
            || normalized == QStringLiteral("orientation")
            || normalized == QStringLiteral("-orient")
            || normalized == QStringLiteral("orient");
    }
    if (canonical == QStringLiteral("-l-size")) {
        return normalized == QStringLiteral("-l-size")
            || normalized == QStringLiteral("l-size")
            || normalized == QStringLiteral("-size")
            || normalized == QStringLiteral("size");
    }
    return normalized == canonical;
}

QString linePointOptionOutputToken(const QString &canonicalOption)
{
    const QString normalized = canonicalOption.trimmed().toLower();
    if (normalized == QStringLiteral("-orientation")) {
        return QStringLiteral("orientation");
    }
    if (normalized == QStringLiteral("-l-size")) {
        return QStringLiteral("l-size");
    }
    return normalized.startsWith(QLatin1Char('-')) ? normalized.mid(1) : normalized;
}

qreal normalizedOrientationDegrees(qreal value)
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

QString formatOrientationDegrees(qreal value)
{
    const qreal normalized = normalizedOrientationDegrees(value);
    const qreal rounded = std::round(normalized);
    if (std::fabs(normalized - rounded) <= 1e-6) {
        return QString::number(static_cast<int>(rounded));
    }

    QString text = QString::number(normalized, 'f', 3);
    while (text.contains(QLatin1Char('.')) && text.endsWith(QLatin1Char('0'))) {
        text.chop(1);
    }
    if (text.endsWith(QLatin1Char('.'))) {
        text.chop(1);
    }
    return text;
}

QString formatLinePointSize(qreal value)
{
    const qreal size = qMax(0.1, value);
    QString text = QString::number(size, 'f', 1);
    while (text.contains(QLatin1Char('.')) && text.endsWith(QLatin1Char('0')) && !text.endsWith(QStringLiteral(".0"))) {
        text.chop(1);
    }
    return text;
}

bool isOptionValueToken(const QString &token)
{
    const QString trimmed = token.trimmed();
    if (!trimmed.startsWith(QLatin1Char('-'))) {
        return true;
    }

    bool numeric = false;
    trimmed.toDouble(&numeric);
    return numeric;
}

bool removeOptionAtTokenIndex(QString *lineText,
                              const TherionParsedLine &parsedLine,
                              int optionTokenIndex)
{
    if (lineText == nullptr
        || optionTokenIndex < 0
        || optionTokenIndex >= parsedLine.tokenSpans.size()) {
        return false;
    }

    int valueTokenIndex = -1;
    if (optionTokenIndex + 1 < parsedLine.tokens.size()) {
        const QString nextToken = parsedLine.tokens.at(optionTokenIndex + 1).trimmed();
        if (isOptionValueToken(nextToken)) {
            valueTokenIndex = optionTokenIndex + 1;
        }
    }

    const TherionParsedToken optionToken = parsedLine.tokenSpans.at(optionTokenIndex);
    int removeStart = optionToken.start;
    int removeEnd = optionToken.start + optionToken.length;
    if (valueTokenIndex >= 0) {
        if (valueTokenIndex >= parsedLine.tokenSpans.size()) {
            return false;
        }
        const TherionParsedToken valueToken = parsedLine.tokenSpans.at(valueTokenIndex);
        removeEnd = valueToken.start + valueToken.length;
    }

    if (removeStart < 0 || removeEnd < removeStart || removeEnd > lineText->size()) {
        return false;
    }

    if (removeStart > 0 && lineText->at(removeStart - 1).isSpace()) {
        --removeStart;
    } else if (removeEnd < lineText->size() && lineText->at(removeEnd).isSpace()) {
        ++removeEnd;
    }

    lineText->remove(removeStart, removeEnd - removeStart);
    return true;
}

bool removeTokenAtTokenIndex(QString *lineText,
                             const TherionParsedLine &parsedLine,
                             int tokenIndex)
{
    if (lineText == nullptr
        || tokenIndex < 0
        || tokenIndex >= parsedLine.tokenSpans.size()) {
        return false;
    }

    const TherionParsedToken token = parsedLine.tokenSpans.at(tokenIndex);
    int removeStart = token.start;
    int removeEnd = token.start + token.length;
    if (removeStart < 0 || removeEnd < removeStart || removeEnd > lineText->size()) {
        return false;
    }

    if (removeStart > 0 && lineText->at(removeStart - 1).isSpace()) {
        --removeStart;
    } else if (removeEnd < lineText->size() && lineText->at(removeEnd).isSpace()) {
        ++removeEnd;
    }

    lineText->remove(removeStart, removeEnd - removeStart);
    return true;
}

bool isSingleTokenOptionWithValue(const QString &token)
{
    const QString trimmed = token.trimmed();
    if (!TherionTokenRules::tokenStartsOption(trimmed)) {
        return false;
    }

    for (const QChar character : trimmed) {
        if (character.isSpace()) {
            return true;
        }
    }

    return false;
}

bool isPlainOptionToken(const QString &token)
{
    const QString trimmed = token.trimmed();
    return TherionTokenRules::tokenStartsOption(trimmed)
        && !isSingleTokenOptionWithValue(trimmed);
}

bool removeMalformedMapObjectOptionTokens(QString *lineText)
{
    if (lineText == nullptr) {
        return false;
    }

    for (;;) {
        const TherionParsedLine parsedLine = TherionDocumentParser::parseLine(*lineText);
        int removeTokenIndex = -1;
        for (int index = 2; index < parsedLine.tokens.size(); ++index) {
            if (!isSingleTokenOptionWithValue(parsedLine.tokens.at(index))) {
                continue;
            }

            const QString previousToken = parsedLine.tokens.value(index - 1);
            if (isPlainOptionToken(previousToken)) {
                continue;
            }

            removeTokenIndex = index;
            break;
        }

        if (removeTokenIndex < 0) {
            return true;
        }

        if (!removeTokenAtTokenIndex(lineText, parsedLine, removeTokenIndex)) {
            return false;
        }
    }
}

QPair<int, int> optionRangeWithBracketedValue(const TherionParsedLine &parsedLine,
                                              int optionIndex,
                                              const QString &lineText);

struct LinePointOptionReference
{
    int lineIndex = -1;
    int optionTokenIndex = -1;
    int valueTokenIndex = -1;
};

struct LinePointOptionTarget
{
    int coordinateLineIndex = -1;
    int optionBlockEndLineIndex = -1;
    int lastCoordinateTokenIndex = -1;
    LinePointOptionReference existingOption;
};

QVector<QPair<int, int>> coordinateTokenPairsForLine(const TherionParsedLine &parsedLine, int startTokenIndex);

std::optional<LinePointOptionTarget> linePointOptionTarget(QStringList *lines,
                                                          int blockStartLineIndex,
                                                          int blockEndLineIndex,
                                                          int sourceVertexIndex,
                                                          const QString &canonicalOption)
{
    if (lines == nullptr
        || blockStartLineIndex < 0
        || blockEndLineIndex <= blockStartLineIndex
        || sourceVertexIndex < 0) {
        return std::nullopt;
    }

    int nextSourceIndex = 0;
    int coordinateLineIndex = -1;
    int lastCoordinateTokenIndex = -1;
    for (int rowIndex = blockStartLineIndex; rowIndex < blockEndLineIndex; ++rowIndex) {
        const TherionParsedLine rowLine = TherionDocumentParser::parseLine(lines->at(rowIndex), rowIndex + 1);
        const int startTokenIndex = rowIndex == blockStartLineIndex ? 1 : 0;
        const QVector<QPair<int, int>> pairs = coordinateTokenPairsForLine(rowLine, startTokenIndex);
        for (const QPair<int, int> &pair : pairs) {
            const int currentSourceIndex = nextSourceIndex++;
            if (currentSourceIndex == sourceVertexIndex) {
                coordinateLineIndex = rowIndex;
                lastCoordinateTokenIndex = pair.second;
            }
        }
        if (coordinateLineIndex >= 0) {
            break;
        }
    }

    if (coordinateLineIndex < 0) {
        return std::nullopt;
    }

    int optionBlockEndLineIndex = blockEndLineIndex;
    for (int rowIndex = coordinateLineIndex + 1; rowIndex < blockEndLineIndex; ++rowIndex) {
        const TherionParsedLine rowLine = TherionDocumentParser::parseLine(lines->at(rowIndex), rowIndex + 1);
        if (!coordinateTokenPairsForLine(rowLine, 0).isEmpty()) {
            optionBlockEndLineIndex = rowIndex;
            break;
        }
    }

    LinePointOptionTarget target;
    target.coordinateLineIndex = coordinateLineIndex;
    target.optionBlockEndLineIndex = optionBlockEndLineIndex;
    target.lastCoordinateTokenIndex = lastCoordinateTokenIndex;

    for (int rowIndex = coordinateLineIndex; rowIndex < optionBlockEndLineIndex; ++rowIndex) {
        const TherionParsedLine rowLine = TherionDocumentParser::parseLine(lines->at(rowIndex), rowIndex + 1);
        const int startTokenIndex = rowIndex == blockStartLineIndex ? 1 : 0;
        for (int tokenIndex = startTokenIndex; tokenIndex < rowLine.tokens.size(); ++tokenIndex) {
            if (!isLinePointOptionToken(rowLine.tokens.at(tokenIndex), canonicalOption)) {
                continue;
            }
            target.existingOption.lineIndex = rowIndex;
            target.existingOption.optionTokenIndex = tokenIndex;
            target.existingOption.valueTokenIndex = -1;
            if (tokenIndex + 1 < rowLine.tokens.size()
                && isOptionValueToken(rowLine.tokens.at(tokenIndex + 1))) {
                target.existingOption.valueTokenIndex = tokenIndex + 1;
            }
        }
    }

    return target;
}

bool linePointNumericOptionRewriteEdits(const QString &contents,
                                        int lineNumber,
                                        int sourceVertexIndex,
                                        const QString &canonicalOption,
                                        bool enabled,
                                        qreal value,
                                        QVector<TherionSourceTextEdit> *edits,
                                        QString *errorMessage)
{
    if (edits == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "No edit target is available.");
        }
        return false;
    }

    edits->clear();

    if (lineNumber <= 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "The selected line number is invalid.");
        }
        return false;
    }
    if (sourceVertexIndex < 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "The selected source vertex index is invalid.");
        }
        return false;
    }

    const QString normalizedOption = canonicalOption.trimmed().toLower();
    if (normalizedOption != QStringLiteral("-orientation") && normalizedOption != QStringLiteral("-l-size")) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "Unsupported line-point option.");
        }
        return false;
    }

    const TherionParsedSourceDocument sourceDocument = TherionDocumentParser::parseSourceDocument(contents);
    QStringList lines;
    lines.reserve(sourceDocument.lines.size());
    for (const TherionParsedSourceLine &sourceLine : sourceDocument.lines) {
        lines.append(sourceLine.text);
    }
    if (lineNumber > lines.size()) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "The selected line no longer exists.");
        }
        return false;
    }

    const int blockStartLineIndex = lineNumber - 1;
    const TherionParsedLine startLine = TherionDocumentParser::parseLine(lines.at(blockStartLineIndex), lineNumber);
    if (startLine.directive != QStringLiteral("line")) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "The selected line is not a writable line block.");
        }
        return false;
    }

    int blockEndLineIndex = -1;
    for (int candidateIndex = blockStartLineIndex + 1; candidateIndex < lines.size(); ++candidateIndex) {
        const TherionParsedLine candidateLine = TherionDocumentParser::parseLine(lines.at(candidateIndex), candidateIndex + 1);
        if (candidateLine.directive == QStringLiteral("endline")) {
            blockEndLineIndex = candidateIndex;
            break;
        }
    }
    if (blockEndLineIndex < 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "The selected line block is missing endline.");
        }
        return false;
    }

    const std::optional<LinePointOptionTarget> target =
        linePointOptionTarget(&lines, blockStartLineIndex, blockEndLineIndex, sourceVertexIndex, normalizedOption);
    if (!target.has_value()) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "The selected line block does not contain the requested vertex.");
        }
        return false;
    }

    const LinePointOptionReference existing = target->existingOption;
    if (!enabled) {
        if (existing.lineIndex < 0) {
            return true;
        }
        QString lineText = lines.at(existing.lineIndex);
        const TherionParsedLine parsedLine = TherionDocumentParser::parseLine(lineText, existing.lineIndex + 1);
        if (!removeOptionAtTokenIndex(&lineText, parsedLine, existing.optionTokenIndex)) {
            if (errorMessage != nullptr) {
                *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "Line-point option could not be removed.");
            }
            return false;
        }
        if (lineText.trimmed().isEmpty()
            && coordinateTokenPairsForLine(parsedLine, existing.lineIndex == blockStartLineIndex ? 1 : 0).isEmpty()) {
            const TherionParsedSourceLine &sourceLine = sourceDocument.lines.at(existing.lineIndex);
            edits->append(TherionSourceTextEdit{
                sourceLine.startOffset,
                sourceLine.textLength + sourceLine.lineEndingLength,
                QString(),
            });
        } else {
            const TherionParsedSourceLine &sourceLine = sourceDocument.lines.at(existing.lineIndex);
            if (lineText != sourceLine.text) {
                edits->append(TherionSourceTextEdit{
                    sourceLine.startOffset,
                    sourceLine.textLength,
                    lineText,
                });
            }
        }
        return true;
    }

    const QString formattedValue = normalizedOption == QStringLiteral("-orientation")
        ? formatOrientationDegrees(value)
        : formatLinePointSize(value);
    if (existing.lineIndex >= 0) {
        QString lineText = lines.at(existing.lineIndex);
        const TherionParsedLine parsedLine = TherionDocumentParser::parseLine(lineText, existing.lineIndex + 1);
        if (existing.valueTokenIndex >= 0 && existing.valueTokenIndex < parsedLine.tokenSpans.size()) {
            const TherionParsedToken valueToken = parsedLine.tokenSpans.at(existing.valueTokenIndex);
            if (valueToken.start < 0
                || valueToken.length < 0
                || valueToken.start + valueToken.length > lineText.size()) {
                if (errorMessage != nullptr) {
                    *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "Line-point option value could not be rewritten.");
                }
                return false;
            }
            lineText.replace(valueToken.start, valueToken.length, formattedValue);
        } else {
            if (existing.optionTokenIndex < 0 || existing.optionTokenIndex >= parsedLine.tokenSpans.size()) {
                if (errorMessage != nullptr) {
                    *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "Line-point option could not be rewritten.");
                }
                return false;
            }
            const TherionParsedToken optionToken = parsedLine.tokenSpans.at(existing.optionTokenIndex);
            if (optionToken.start < 0
                || optionToken.length < 0
                || optionToken.start + optionToken.length > lineText.size()) {
                if (errorMessage != nullptr) {
                    *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "Line-point option could not be rewritten.");
                }
                return false;
            }
            lineText.insert(optionToken.start + optionToken.length,
                            QStringLiteral(" %1").arg(formattedValue));
        }
        const TherionParsedSourceLine &sourceLine = sourceDocument.lines.at(existing.lineIndex);
        if (lineText != sourceLine.text) {
            edits->append(TherionSourceTextEdit{
                sourceLine.startOffset,
                sourceLine.textLength,
                lineText,
            });
        }
        return true;
    }

    const QString coordinateLine = lines.at(target->coordinateLineIndex);
    const QRegularExpression leadingWhitespacePattern(QStringLiteral(R"(^\s*)"));
    const QRegularExpressionMatch indentMatch = leadingWhitespacePattern.match(coordinateLine);
    const QString indent = indentMatch.hasMatch() && !indentMatch.captured(0).isEmpty()
        ? indentMatch.captured(0)
        : QStringLiteral("  ");
    if (target->optionBlockEndLineIndex < 0 || target->optionBlockEndLineIndex >= sourceDocument.lines.size()) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "Line-point option insertion range could not be rewritten.");
        }
        return false;
    }
    QString insertionLineEnding = sourceDocument.lines.value(target->coordinateLineIndex).lineEnding;
    if (insertionLineEnding.isEmpty()) {
        insertionLineEnding = TherionSourceText::detectedLineEnding(contents);
    }
    const QString insertedLine = QStringLiteral("%1%2 %3%4")
                                     .arg(indent, linePointOptionOutputToken(normalizedOption), formattedValue, insertionLineEnding);
    edits->append(TherionSourceTextEdit{
        sourceDocument.lines.at(target->optionBlockEndLineIndex).startOffset,
        0,
        insertedLine,
    });
    return true;
}

int optionTokenIndex(const TherionParsedLine &parsedLine, const QString &optionName)
{
    const QString normalizedOption = optionName.trimmed().toLower();
    for (int index = 1; index < parsedLine.tokens.size(); ++index) {
        if (parsedLine.tokens.at(index).trimmed().toLower() == normalizedOption) {
            return index;
        }
    }

    return -1;
}

bool replaceTokenText(QString *lineText, const TherionParsedLine &parsedLine, int tokenIndex, const QString &replacement)
{
    if (lineText == nullptr
        || tokenIndex < 0
        || tokenIndex >= parsedLine.tokenSpans.size()) {
        return false;
    }

    const TherionParsedToken token = parsedLine.tokenSpans.at(tokenIndex);
    if (token.start < 0 || token.length < 0 || token.start + token.length > lineText->size()) {
        return false;
    }

    lineText->replace(token.start, token.length, replacement);
    return true;
}

bool upsertSingleValueOption(QString *lineText, const QString &optionName, const QString &value)
{
    if (lineText == nullptr || optionName.trimmed().isEmpty()) {
        return false;
    }

    const QString normalizedOption = optionName.trimmed().startsWith(QLatin1Char('-'))
        ? optionName.trimmed()
        : QStringLiteral("-%1").arg(optionName.trimmed());
    TherionParsedLine parsedLine = TherionDocumentParser::parseLine(*lineText);
    const int existingOptionIndex = optionTokenIndex(parsedLine, normalizedOption);
    const QString serializedValue = serializedInlineToken(value);
    if (serializedValue.isEmpty()) {
        if (existingOptionIndex < 0) {
            return true;
        }
        return removeOptionAtTokenIndex(lineText, parsedLine, existingOptionIndex);
    }

    if (existingOptionIndex >= 0) {
        if (existingOptionIndex + 1 < parsedLine.tokens.size()
            && existingOptionIndex + 1 < parsedLine.tokenSpans.size()
            && !parsedLine.tokens.at(existingOptionIndex + 1).startsWith(QLatin1Char('-'))) {
            return replaceTokenText(lineText, parsedLine, existingOptionIndex + 1, serializedValue);
        }
        if (existingOptionIndex >= parsedLine.tokenSpans.size()) {
            return false;
        }
        const TherionParsedToken optionToken = parsedLine.tokenSpans.at(existingOptionIndex);
        if (optionToken.start < 0 || optionToken.length < 0 || optionToken.start + optionToken.length > lineText->size()) {
            return false;
        }
        lineText->insert(optionToken.start + optionToken.length, QStringLiteral(" ") + serializedValue);
        return true;
    }

    const QString insertionText = QStringLiteral("%1 %2").arg(normalizedOption, serializedValue);
    if (parsedLine.commentStart >= 0) {
        int insertIndex = parsedLine.commentStart;
        while (insertIndex > 0 && lineText->at(insertIndex - 1).isSpace()) {
            --insertIndex;
        }
        lineText->insert(insertIndex, QStringLiteral(" ") + insertionText);
    } else {
        *lineText += QStringLiteral(" ") + insertionText;
    }
    return true;
}

bool upsertMapObjectValueOption(QString *lineText, const QString &value)
{
    if (lineText == nullptr) {
        return false;
    }

    TherionParsedLine parsedLine = TherionDocumentParser::parseLine(*lineText);
    const int existingOptionIndex = optionTokenIndex(parsedLine, QStringLiteral("-value"));
    const QString trimmedValue = value.trimmed();
    const QString serializedValue = trimmedValue.isEmpty()
        ? QString()
        : serializeTherionArgumentToken(trimmedValue);
    if (serializedValue.isEmpty()) {
        if (existingOptionIndex < 0) {
            return true;
        }
        const QPair<int, int> range = optionRangeWithBracketedValue(parsedLine, existingOptionIndex, *lineText);
        if (range.first < 0 || range.second < range.first || range.second > lineText->size()) {
            return false;
        }
        lineText->remove(range.first, range.second - range.first);
        return true;
    }

    if (existingOptionIndex >= 0) {
        const QPair<int, int> range = optionRangeWithBracketedValue(parsedLine, existingOptionIndex, *lineText);
        if (range.first < 0 || range.second < range.first || range.second > lineText->size()) {
            return false;
        }

        int valueStart = parsedLine.tokenSpans.value(existingOptionIndex).start
            + parsedLine.tokenSpans.value(existingOptionIndex).length;
        while (valueStart < range.second && lineText->at(valueStart).isSpace()) {
            ++valueStart;
        }
        if (valueStart > range.second) {
            return false;
        }
        if (valueStart == range.second) {
            lineText->insert(valueStart, QStringLiteral(" ") + serializedValue);
            return true;
        }
        lineText->replace(valueStart, range.second - valueStart, serializedValue);
        return true;
    }

    const QString insertionText = QStringLiteral("-value %1").arg(serializedValue);
    if (parsedLine.commentStart >= 0) {
        int insertIndex = parsedLine.commentStart;
        while (insertIndex > 0 && lineText->at(insertIndex - 1).isSpace()) {
            --insertIndex;
        }
        lineText->insert(insertIndex, QStringLiteral(" ") + insertionText);
    } else {
        *lineText += QStringLiteral(" ") + insertionText;
    }
    return true;
}

QPair<int, int> optionRangeWithBracketedValue(const TherionParsedLine &parsedLine, int optionIndex, const QString &lineText)
{
    if (optionIndex < 0 || optionIndex >= parsedLine.tokenSpans.size()) {
        return qMakePair(-1, -1);
    }

    const TherionParsedToken optionToken = parsedLine.tokenSpans.at(optionIndex);
    int rangeStart = optionToken.start;
    int rangeEnd = optionToken.start + optionToken.length;
    if (rangeStart < 0 || rangeEnd < rangeStart || rangeEnd > lineText.size()) {
        return qMakePair(-1, -1);
    }

    int valueEndTokenIndex = -1;
    if (optionIndex + 1 < parsedLine.tokens.size()) {
        valueEndTokenIndex = optionIndex + 1;
        const QString firstValueToken = parsedLine.tokens.at(valueEndTokenIndex);
        if (firstValueToken.contains(QLatin1Char('[')) && !firstValueToken.contains(QLatin1Char(']'))) {
            for (int index = valueEndTokenIndex + 1; index < parsedLine.tokens.size(); ++index) {
                const QString token = parsedLine.tokens.at(index);
                if (TherionTokenRules::tokenStartsOption(token)) {
                    break;
                }
                valueEndTokenIndex = index;
                if (token.contains(QLatin1Char(']'))) {
                    break;
                }
            }
        } else if (!firstValueToken.contains(QLatin1Char('['))) {
            for (int index = valueEndTokenIndex + 1; index < parsedLine.tokens.size(); ++index) {
                const QString token = parsedLine.tokens.at(index);
                if (TherionTokenRules::tokenStartsOption(token)) {
                    break;
                }
                valueEndTokenIndex = index;
            }
        }
    }

    if (valueEndTokenIndex >= 0 && valueEndTokenIndex < parsedLine.tokenSpans.size()) {
        const TherionParsedToken valueToken = parsedLine.tokenSpans.at(valueEndTokenIndex);
        rangeEnd = valueToken.start + valueToken.length;
    }

    if (rangeStart > 0 && lineText.at(rangeStart - 1).isSpace()) {
        --rangeStart;
    } else if (rangeEnd < lineText.size() && lineText.at(rangeEnd).isSpace()) {
        ++rangeEnd;
    }

    return qMakePair(rangeStart, rangeEnd);
}

QPair<int, int> coordinateTokenPair(const TherionParsedLine &parsedLine)
{
    int firstIndex = -1;
    int secondIndex = -1;
    for (int index = 1; index < parsedLine.tokens.size(); ++index) {
        if (!TherionTokenRules::isNumericToken(parsedLine.tokens.at(index))) {
            continue;
        }

        if (index < parsedLine.tokenSpans.size()
            && parsedLine.tokenSpans.at(index).type == TherionTokenType::QuotedString) {
            continue;
        }

        if (firstIndex < 0) {
            firstIndex = index;
            continue;
        }

        secondIndex = index;
        break;
    }

    return qMakePair(firstIndex, secondIndex);
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

QString sanitizeObjectIdentifier(const QString &candidate, const QString &fallbackPrefix)
{
    QString source = candidate.trimmed().toLower();
    if (source.isEmpty()) {
        source = fallbackPrefix.trimmed().toLower();
    }
    if (source.isEmpty()) {
        source = QStringLiteral("object");
    }

    QString normalized;
    normalized.reserve(source.size());
    bool previousDash = false;
    for (const QChar character : source) {
        const bool allowed = character.isLetterOrNumber()
            || character == QLatin1Char('-')
            || character == QLatin1Char('_')
            || character == QLatin1Char('.');
        if (allowed) {
            normalized.append(character);
            previousDash = false;
            continue;
        }
        if (!previousDash) {
            normalized.append(QLatin1Char('-'));
            previousDash = true;
        }
    }

    while (normalized.startsWith(QLatin1Char('-'))) {
        normalized.remove(0, 1);
    }
    while (normalized.endsWith(QLatin1Char('-'))) {
        normalized.chop(1);
    }

    if (normalized.isEmpty()) {
        return fallbackPrefix.isEmpty() ? QStringLiteral("object") : fallbackPrefix;
    }

    return normalized;
}

QString uniqueObjectIdentifier(const QString &baseIdentifier, const QSet<QString> &existingIdentifiers)
{
    const QString base = sanitizeObjectIdentifier(baseIdentifier, QStringLiteral("object"));
    if (!existingIdentifiers.contains(base.toLower())) {
        return base;
    }

    int suffix = 2;
    while (true) {
        const QString candidate = QStringLiteral("%1-%2").arg(base).arg(suffix++);
        if (!existingIdentifiers.contains(candidate.toLower())) {
            return candidate;
        }
    }
}

int lastEndscrapLineIndex(const QStringList &lines)
{
    int foundIndex = -1;
    for (int index = 0; index < lines.size(); ++index) {
        const TherionParsedLine parsedLine = TherionDocumentParser::parseLine(lines.at(index), index + 1);
        if (parsedLine.directive == QStringLiteral("endscrap")) {
            foundIndex = index;
        }
    }

    return foundIndex;
}

int matchingEndscrapIndex(const QStringList &lines, int scrapStartIndex)
{
    if (scrapStartIndex < 0 || scrapStartIndex >= lines.size()) {
        return -1;
    }

    int depth = 0;
    for (int index = scrapStartIndex; index < lines.size(); ++index) {
        const TherionParsedLine parsedLine = TherionDocumentParser::parseLine(lines.at(index), index + 1);
        if (parsedLine.directive == QStringLiteral("scrap")) {
            ++depth;
            continue;
        }
        if (parsedLine.directive != QStringLiteral("endscrap")) {
            continue;
        }
        --depth;
        if (depth == 0) {
            return index;
        }
    }

    return -1;
}

struct UnclosedMapBlock
{
    QString directive;
    QString closeDirective;
    int lineNumber = 0;
};

UnclosedMapBlock firstUnclosedMapBlock(const QStringList &lines)
{
    QVector<UnclosedMapBlock> openBlocks;
    for (int index = 0; index < lines.size(); ++index) {
        const TherionParsedLine parsedLine = TherionDocumentParser::parseLine(lines.at(index), index + 1);
        if (parsedLine.directive == QStringLiteral("scrap")) {
            openBlocks.append(UnclosedMapBlock{
                QStringLiteral("scrap"),
                QStringLiteral("endscrap"),
                index + 1,
            });
            continue;
        }
        if (parsedLine.directive == QStringLiteral("line")) {
            openBlocks.append(UnclosedMapBlock{
                QStringLiteral("line"),
                QStringLiteral("endline"),
                index + 1,
            });
            continue;
        }
        if (parsedLine.directive == QStringLiteral("area")) {
            openBlocks.append(UnclosedMapBlock{
                QStringLiteral("area"),
                QStringLiteral("endarea"),
                index + 1,
            });
            continue;
        }
        if (parsedLine.directive == QStringLiteral("endline")) {
            if (!openBlocks.isEmpty() && openBlocks.constLast().directive == QStringLiteral("line")) {
                openBlocks.removeLast();
            }
            continue;
        }
        if (parsedLine.directive == QStringLiteral("endarea")) {
            if (!openBlocks.isEmpty() && openBlocks.constLast().directive == QStringLiteral("area")) {
                openBlocks.removeLast();
            }
            continue;
        }
        if (parsedLine.directive == QStringLiteral("endscrap")) {
            if (!openBlocks.isEmpty() && openBlocks.constLast().directive == QStringLiteral("scrap")) {
                openBlocks.removeLast();
                continue;
            }
            if (!openBlocks.isEmpty()) {
                return openBlocks.constLast();
            }
        }
    }

    return openBlocks.isEmpty() ? UnclosedMapBlock{} : openBlocks.constLast();
}

bool rejectDraftInsertionIntoUnclosedScrap(const QStringList &lines, QString *errorMessage)
{
    const UnclosedMapBlock unclosedBlock = firstUnclosedMapBlock(lines);
    if (unclosedBlock.lineNumber <= 0) {
        return false;
    }

    if (errorMessage != nullptr) {
        *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor",
                                                    "Cannot insert draft geometry while %1 block at line %2 is missing %3.")
            .arg(unclosedBlock.directive)
            .arg(unclosedBlock.lineNumber)
            .arg(unclosedBlock.closeDirective);
    }
    return true;
}

int endscrapLineIndexForScrapIdentifier(const QStringList &lines, const QString &scrapIdentifier)
{
    const QString normalizedIdentifier = scrapIdentifier.trimmed();
    if (normalizedIdentifier.isEmpty()) {
        return -1;
    }

    for (int index = 0; index < lines.size(); ++index) {
        const TherionParsedLine parsedLine = TherionDocumentParser::parseLine(lines.at(index), index + 1);
        if (parsedLine.directive != QStringLiteral("scrap")
            || parsedLine.tokens.value(1).compare(normalizedIdentifier, Qt::CaseInsensitive) != 0) {
            continue;
        }
        return matchingEndscrapIndex(lines, index);
    }

    return -1;
}

int draftInsertionEndscrapLineIndex(const QStringList &lines,
                                    const TherionDraftObjectOptions &objectOptions,
                                    QString *errorMessage)
{
    const QString targetScrapIdentifier = objectOptions.targetScrapIdentifier.trimmed();
    if (!targetScrapIdentifier.isEmpty()) {
        const int targetedIndex = endscrapLineIndexForScrapIdentifier(lines, targetScrapIdentifier);
        if (targetedIndex >= 0) {
            return targetedIndex;
        }
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor",
                                                        "Target scrap '%1' no longer exists.")
                .arg(targetScrapIdentifier);
        }
        return -1;
    }

    return lastEndscrapLineIndex(lines);
}

}

bool TherionDocumentEditor::applySourceTextEdits(QString *contents,
                                                 QVector<TherionSourceTextEdit> edits,
                                                 QString *errorMessage)
{
    if (contents == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "No document contents are available.");
        }
        return false;
    }
    if (edits.isEmpty()) {
        return true;
    }

    std::sort(edits.begin(), edits.end(), [](const TherionSourceTextEdit &left, const TherionSourceTextEdit &right) {
        return left.startOffset > right.startOffset;
    });

    int nextStartOffset = contents->size() + 1;
    for (const TherionSourceTextEdit &edit : edits) {
        if (edit.startOffset < 0
            || edit.length < 0
            || edit.startOffset + edit.length > contents->size()
            || edit.startOffset + edit.length > nextStartOffset) {
            if (errorMessage != nullptr) {
                *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "Source edit range is invalid.");
            }
            return false;
        }
        nextStartOffset = edit.startOffset;
    }

    for (const TherionSourceTextEdit &edit : edits) {
        contents->replace(edit.startOffset, edit.length, edit.replacementText);
    }
    return true;
}

bool TherionDocumentEditor::structureEntryNameRewriteEdits(const QString &contents,
                                                           int lineNumber,
                                                           const QString &category,
                                                           const QString &newName,
                                                           QVector<TherionSourceTextEdit> *edits,
                                                           QString *errorMessage)
{
    if (edits == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "No source edit output is available.");
        }
        return false;
    }

    const QString trimmedName = newName.trimmed();
    if (trimmedName.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "The new structure name is empty.");
        }
        return false;
    }

    if (lineNumber <= 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "The selected line number is invalid.");
        }
        return false;
    }

    const TherionSourceDocument sourceDocument = TherionSourceDocument::fromText(contents);
    const TherionSourceDocumentLine *sourceDocumentLine = sourceDocument.lineAtLineNumber(lineNumber);
    if (sourceDocumentLine == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "The selected line no longer exists.");
        }
        return false;
    }

    const TherionParsedSourceLine &sourceLine = sourceDocumentLine->sourceLine;
    const QString &lineText = sourceLine.text;
    const TherionParsedLine &parsedLine = sourceLine.parsed;
    if (parsedLine.tokens.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "The selected line does not contain a structure object.");
        }
        return false;
    }

    const int tokenIndex = nameTokenIndexForCategory(category, parsedLine);
    if (tokenIndex < 0 || tokenIndex >= parsedLine.tokenSpans.size()) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "The selected structure item cannot be rewritten.");
        }
        return false;
    }

    const TherionParsedToken tokenSpan = parsedLine.tokenSpans.at(tokenIndex);
    if (tokenSpan.start < 0 || tokenSpan.length < 0 || tokenSpan.start + tokenSpan.length > lineText.size()) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "The selected line could not be rewritten.");
        }
        return false;
    }

    const QString replacementToken = replacementTokenForLine(trimmedName, lineText, tokenSpan);
    edits->append(TherionSourceTextEdit{
        sourceLine.absoluteTokenStart(tokenSpan),
        tokenSpan.length,
        replacementToken,
    });
    return true;
}

bool TherionDocumentEditor::appendScrapBlockEdits(const QString &contents,
                                                  const QString &preferredName,
                                                  QVector<TherionSourceTextEdit> *edits,
                                                  int *insertedLineNumber,
                                                  QString *errorMessage,
                                                  const QString &options)
{
    if (edits == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "No source edit output is available.");
        }
        return false;
    }
    edits->clear();

    const QString lineEnding = contents.contains(QStringLiteral("\r\n")) ? QStringLiteral("\r\n") : QStringLiteral("\n");
    const QVector<TherionParsedLine> parsedLines = TherionDocumentParser::parseTokenLines(contents);

    QSet<QString> existingNames;
    for (const TherionParsedLine &parsedLine : parsedLines) {
        if (parsedLine.directive != QStringLiteral("scrap")) {
            continue;
        }
        if (parsedLine.tokens.size() < 2) {
            continue;
        }

        existingNames.insert(parsedLine.tokens.at(1).trimmed().toLower());
    }

    QString resolvedName;
    const QString sanitizedPreferredName = sanitizeScrapName(preferredName);
    if (!preferredName.trimmed().isEmpty()
        && sanitizedPreferredName != QStringLiteral("map-draft")
        && sanitizedPreferredName != QStringLiteral("new-scrap")) {
        resolvedName = uniqueObjectIdentifier(sanitizedPreferredName, existingNames);
    } else {
        resolvedName = generatedIdentifier(QStringLiteral("scrap"), existingNames);
    }

    QString appendedText;
    QString lineCountText = contents;
    if (!lineCountText.isEmpty() && !lineCountText.endsWith(QLatin1Char('\n'))) {
        appendedText += lineEnding;
        lineCountText += lineEnding;
    }
    if (!lineCountText.isEmpty()) {
        appendedText += lineEnding;
        lineCountText += lineEnding;
    }

    const int scrapLineNumber = lineCountForText(lineCountText) + 1;
    appendedText += QStringLiteral("scrap %1").arg(resolvedName);
    const QString normalizedOptions = options.trimmed();
    if (!normalizedOptions.isEmpty()) {
        appendedText += QLatin1Char(' ');
        appendedText += normalizedOptions;
    }
    appendedText += lineEnding;
    appendedText += QStringLiteral("endscrap");
    appendedText += lineEnding;

    edits->append(TherionSourceTextEdit{
        static_cast<int>(contents.size()),
        0,
        appendedText,
    });

    if (insertedLineNumber != nullptr) {
        *insertedLineNumber = scrapLineNumber;
    }

    return true;
}

bool TherionDocumentEditor::appendDraftGeometryEdits(const QString &contents,
                                                     const QString &kind,
                                                     const QVector<QPointF> &vertices,
                                                     QVector<TherionSourceTextEdit> *edits,
                                                     int *insertedLineNumber,
                                                     QString *errorMessage,
                                                     const TherionDraftObjectOptions &objectOptions)
{
    if (edits == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "No source edit output is available.");
        }
        return false;
    }
    edits->clear();

    const QString normalizedKind = kind.trimmed().toLower();
    if (normalizedKind != QStringLiteral("point")
        && normalizedKind != QStringLiteral("line")
        && normalizedKind != QStringLiteral("area")) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "Unsupported draft geometry kind.");
        }
        return false;
    }

    const int minimumVertices = normalizedKind == QStringLiteral("point")
        ? 1
        : (normalizedKind == QStringLiteral("line") ? 2 : 3);
    if (vertices.size() < minimumVertices) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "Draft geometry does not contain enough vertices.");
        }
        return false;
    }

    QString updated = contents;
    const QStringList originalLines = splitLinesTrimmingCarriageReturns(updated);
    if (rejectDraftInsertionIntoUnclosedScrap(originalLines, errorMessage)) {
        return false;
    }
    const bool needsFallbackScrap = draftInsertionEndscrapLineIndex(originalLines, objectOptions, nullptr) < 0
        && objectOptions.targetScrapIdentifier.trimmed().isEmpty();
    if (needsFallbackScrap) {
        QVector<TherionSourceTextEdit> fallbackEdits;
        if (!appendScrapBlockEdits(updated, QStringLiteral("map-draft"), &fallbackEdits, nullptr, errorMessage)
            || !applySourceTextEdits(&updated, fallbackEdits, errorMessage)) {
            return false;
        }
    }

    QStringList lines = splitLinesTrimmingCarriageReturns(updated);
    int insertionIndex = draftInsertionEndscrapLineIndex(lines, objectOptions, errorMessage);

    if (insertionIndex < 0 && needsFallbackScrap) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "Unable to resolve a scrap insertion target.");
        }
        return false;
    } else if (insertionIndex < 0) {
        return false;
    }

    QStringList geometryLines;
    int insertionLineOffset = 0;
    if (normalizedKind == QStringLiteral("point")) {
        const QPointF anchor = vertices.first();
        TherionDraftObjectOptions pointOptions = objectOptions;
        if (pointOptions.type.trimmed().isEmpty()) {
            pointOptions.type = QStringLiteral("station");
        }
        geometryLines.append(QStringLiteral("  point %1 %2 %3")
                                 .arg(formatCoordinate(anchor.x()),
                                      formatCoordinate(anchor.y()),
                                      draftObjectHeader(QString(), QStringLiteral("station"), pointOptions, true).trimmed()));
    } else if (normalizedKind == QStringLiteral("line")) {
        geometryLines.append(QStringLiteral("  %1")
                                 .arg(draftObjectHeader(QStringLiteral("line"), QStringLiteral("wall"), objectOptions)));
        for (const QPointF &vertex : vertices) {
            geometryLines.append(QStringLiteral("    %1 %2")
                                     .arg(formatCoordinate(vertex.x()), formatCoordinate(vertex.y())));
        }
        geometryLines.append(QStringLiteral("  endline"));
    } else {
        const int scrapStartIndex = matchingScrapStartIndex(lines, insertionIndex);
        if (scrapStartIndex < 0) {
            if (errorMessage != nullptr) {
                *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "Unable to resolve scrap boundaries for area insertion.");
            }
            return false;
        }
        const QSet<QString> existingIdentifiers = identifiersInsideScrap(lines, scrapStartIndex, insertionIndex);
        const QString borderIdentifier = generatedIdentifier(QStringLiteral("line"), existingIdentifiers);

        geometryLines.append(QStringLiteral("  line border -id %1 -close on").arg(borderIdentifier));
        for (const QPointF &vertex : vertices) {
            geometryLines.append(QStringLiteral("    %1 %2")
                                     .arg(formatCoordinate(vertex.x()), formatCoordinate(vertex.y())));
        }
        geometryLines.append(QStringLiteral("  endline"));
        insertionLineOffset = geometryLines.size();
        geometryLines.append(QStringLiteral("  %1")
                                 .arg(draftObjectHeader(QStringLiteral("area"), QStringLiteral("water"), objectOptions)));
        geometryLines.append(QStringLiteral("    %1").arg(borderIdentifier));
        geometryLines.append(QStringLiteral("  endarea"));
    }

    if (needsFallbackScrap) {
        if (!insertPhysicalSourceLines(&updated, insertionIndex, geometryLines, errorMessage)) {
            return false;
        }
        edits->append(TherionSourceTextEdit{
            static_cast<int>(contents.size()),
            0,
            updated.mid(contents.size()),
        });
    } else {
        TherionSourceTextEdit edit;
        if (!physicalSourceLineInsertionEdit(updated, insertionIndex, geometryLines, &edit, errorMessage)) {
            return false;
        }
        edits->append(edit);
    }

    if (insertedLineNumber != nullptr) {
        *insertedLineNumber = insertionIndex + 1 + insertionLineOffset;
    }

    return true;
}

bool TherionDocumentEditor::appendDraftLineGeometryEdits(const QString &contents,
                                                         const QStringList &coordinateRows,
                                                         QVector<TherionSourceTextEdit> *edits,
                                                         int *insertedLineNumber,
                                                         QString *errorMessage,
                                                         const QString &lineOptions,
                                                         const TherionDraftObjectOptions &objectOptions)
{
    if (edits == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "No source edit output is available.");
        }
        return false;
    }
    edits->clear();

    QStringList normalizedRows;
    normalizedRows.reserve(coordinateRows.size());
    for (const QString &row : coordinateRows) {
        const QString trimmedRow = row.trimmed();
        if (!trimmedRow.isEmpty()) {
            normalizedRows.append(trimmedRow);
        }
    }

    if (normalizedRows.size() < 2) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "Draft line geometry does not contain enough coordinate rows.");
        }
        return false;
    }

    QString updated = contents;
    const QStringList originalLines = splitLinesTrimmingCarriageReturns(updated);
    if (rejectDraftInsertionIntoUnclosedScrap(originalLines, errorMessage)) {
        return false;
    }
    const bool needsFallbackScrap = draftInsertionEndscrapLineIndex(originalLines, objectOptions, nullptr) < 0
        && objectOptions.targetScrapIdentifier.trimmed().isEmpty();
    if (needsFallbackScrap) {
        QVector<TherionSourceTextEdit> fallbackEdits;
        if (!appendScrapBlockEdits(updated, QStringLiteral("map-draft"), &fallbackEdits, nullptr, errorMessage)
            || !applySourceTextEdits(&updated, fallbackEdits, errorMessage)) {
            return false;
        }
    }

    QStringList lines = splitLinesTrimmingCarriageReturns(updated);
    int insertionIndex = draftInsertionEndscrapLineIndex(lines, objectOptions, errorMessage);

    if (insertionIndex < 0 && needsFallbackScrap) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "Unable to resolve a scrap insertion target.");
        }
        return false;
    } else if (insertionIndex < 0) {
        return false;
    }

    QStringList geometryLines;
    const QString normalizedLineOptions = lineOptions.trimmed();
    const QString lineHeader = draftObjectHeader(QStringLiteral("line"), QStringLiteral("wall"), objectOptions);
    if (normalizedLineOptions.isEmpty()) {
        geometryLines.append(QStringLiteral("  %1").arg(lineHeader));
    } else {
        geometryLines.append(QStringLiteral("  %1 %2").arg(lineHeader, normalizedLineOptions));
    }
    for (const QString &row : std::as_const(normalizedRows)) {
        geometryLines.append(QStringLiteral("    %1").arg(row));
    }
    geometryLines.append(QStringLiteral("  endline"));

    if (needsFallbackScrap) {
        if (!insertPhysicalSourceLines(&updated, insertionIndex, geometryLines, errorMessage)) {
            return false;
        }
        edits->append(TherionSourceTextEdit{
            static_cast<int>(contents.size()),
            0,
            updated.mid(contents.size()),
        });
    } else {
        TherionSourceTextEdit edit;
        if (!physicalSourceLineInsertionEdit(updated, insertionIndex, geometryLines, &edit, errorMessage)) {
            return false;
        }
        edits->append(edit);
    }

    if (insertedLineNumber != nullptr) {
        *insertedLineNumber = insertionIndex + 1;
    }

    return true;
}

bool TherionDocumentEditor::appendDraftAreaGeometryEdits(const QString &contents,
                                                         const QStringList &coordinateRows,
                                                         QVector<TherionSourceTextEdit> *edits,
                                                         int *insertedLineNumber,
                                                         QString *errorMessage,
                                                         const TherionDraftObjectOptions &objectOptions)
{
    if (edits == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "No source edit output is available.");
        }
        return false;
    }
    edits->clear();

    QStringList normalizedRows;
    normalizedRows.reserve(coordinateRows.size());
    for (const QString &row : coordinateRows) {
        const QString trimmedRow = row.trimmed();
        if (!trimmedRow.isEmpty()) {
            normalizedRows.append(trimmedRow);
        }
    }

    if (normalizedRows.size() < 3) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "Draft area geometry does not contain enough coordinate rows.");
        }
        return false;
    }

    QString updated = contents;
    const QStringList originalLines = splitLinesTrimmingCarriageReturns(updated);
    if (rejectDraftInsertionIntoUnclosedScrap(originalLines, errorMessage)) {
        return false;
    }
    const bool needsFallbackScrap = draftInsertionEndscrapLineIndex(originalLines, objectOptions, nullptr) < 0
        && objectOptions.targetScrapIdentifier.trimmed().isEmpty();
    if (needsFallbackScrap) {
        QVector<TherionSourceTextEdit> fallbackEdits;
        if (!appendScrapBlockEdits(updated, QStringLiteral("map-draft"), &fallbackEdits, nullptr, errorMessage)
            || !applySourceTextEdits(&updated, fallbackEdits, errorMessage)) {
            return false;
        }
    }

    QStringList lines = splitLinesTrimmingCarriageReturns(updated);
    int insertionIndex = draftInsertionEndscrapLineIndex(lines, objectOptions, errorMessage);

    if (insertionIndex < 0 && needsFallbackScrap) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "Unable to resolve a scrap insertion target.");
        }
        return false;
    } else if (insertionIndex < 0) {
        return false;
    }

    const int scrapStartIndex = matchingScrapStartIndex(lines, insertionIndex);
    if (scrapStartIndex < 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "Unable to resolve scrap boundaries for area insertion.");
        }
        return false;
    }
    const QSet<QString> existingIdentifiers = identifiersInsideScrap(lines, scrapStartIndex, insertionIndex);

    const QString borderIdentifier = generatedIdentifier(QStringLiteral("line"), existingIdentifiers);

    QStringList geometryLines;
    geometryLines.append(QStringLiteral("  line border -id %1 -close on").arg(borderIdentifier));
    for (const QString &row : std::as_const(normalizedRows)) {
        geometryLines.append(QStringLiteral("    %1").arg(row));
    }
    geometryLines.append(QStringLiteral("  endline"));
    geometryLines.append(QStringLiteral("  %1")
                             .arg(draftObjectHeader(QStringLiteral("area"), QStringLiteral("water"), objectOptions)));
    geometryLines.append(QStringLiteral("    %1").arg(borderIdentifier));
    geometryLines.append(QStringLiteral("  endarea"));

    if (needsFallbackScrap) {
        if (!insertPhysicalSourceLines(&updated, insertionIndex, geometryLines, errorMessage)) {
            return false;
        }
        edits->append(TherionSourceTextEdit{
            static_cast<int>(contents.size()),
            0,
            updated.mid(contents.size()),
        });
    } else {
        TherionSourceTextEdit edit;
        if (!physicalSourceLineInsertionEdit(updated, insertionIndex, geometryLines, &edit, errorMessage)) {
            return false;
        }
        edits->append(edit);
    }

    if (insertedLineNumber != nullptr) {
        *insertedLineNumber = insertionIndex + 1 + normalizedRows.size() + 2;
    }

    return true;
}

bool TherionDocumentEditor::appendReferencedAreaEdits(const QString &contents,
                                                      int scrapLineNumber,
                                                      const QVector<TherionReferencedAreaBoundaryLine> &boundaryLines,
                                                      QVector<TherionSourceTextEdit> *edits,
                                                      int *insertedLineNumber,
                                                      QString *errorMessage,
                                                      const TherionDraftObjectOptions &objectOptions)
{
    if (edits == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "No source edit output is available.");
        }
        return false;
    }
    edits->clear();
    if (scrapLineNumber <= 0 || boundaryLines.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "Smart Area has no resolved boundary lines.");
        }
        return false;
    }

    const TherionParsedSourceDocument sourceDocument = TherionDocumentParser::parseSourceDocument(contents);
    QStringList lines;
    lines.reserve(sourceDocument.lines.size());
    for (const TherionParsedSourceLine &sourceLine : sourceDocument.lines) {
        lines.append(sourceLine.text);
    }
    const int scrapStartIndex = scrapLineNumber - 1;
    if (scrapStartIndex < 0 || scrapStartIndex >= lines.size()) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "Smart Area target scrap no longer exists.");
        }
        return false;
    }

    const TherionParsedLine scrapLine = TherionDocumentParser::parseLine(lines.at(scrapStartIndex), scrapLineNumber);
    if (scrapLine.directive != QStringLiteral("scrap")) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "Smart Area target is not a scrap.");
        }
        return false;
    }

    const int insertionIndex = matchingEndscrapIndex(lines, scrapStartIndex);
    if (insertionIndex < 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "Smart Area target scrap is missing endscrap.");
        }
        return false;
    }

    QSet<QString> existingIdentifiers = identifiersInsideScrap(lines, scrapStartIndex, insertionIndex);
    QSet<int> seenLineNumbers;
    QStringList areaReferences;
    for (const TherionReferencedAreaBoundaryLine &boundaryLine : boundaryLines) {
        if (boundaryLine.lineNumber <= scrapLineNumber
            || boundaryLine.lineNumber > insertionIndex
            || seenLineNumbers.contains(boundaryLine.lineNumber)) {
            continue;
        }
        seenLineNumbers.insert(boundaryLine.lineNumber);

        QString lineText = lines.at(boundaryLine.lineNumber - 1);
        const TherionParsedLine parsedLine = TherionDocumentParser::parseLine(lineText, boundaryLine.lineNumber);
        if (parsedLine.directive != QStringLiteral("line")) {
            if (errorMessage != nullptr) {
                *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "Smart Area boundary no longer resolves to a line.");
            }
            return false;
        }

        QString identifier = boundaryLine.identifier.trimmed();
        if (identifier.isEmpty()) {
            identifier = generatedIdentifier(QStringLiteral("line"), existingIdentifiers);
            existingIdentifiers.insert(identifier.toLower());
            if (!upsertSingleValueOption(&lineText, QStringLiteral("-id"), identifier)) {
                if (errorMessage != nullptr) {
                    *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "Smart Area could not add a missing boundary line id.");
                }
                return false;
            }
            lines[boundaryLine.lineNumber - 1] = lineText;
            const TherionParsedSourceLine &boundarySourceLine = sourceDocument.lines.at(boundaryLine.lineNumber - 1);
            edits->append(TherionSourceTextEdit{
                boundarySourceLine.startOffset,
                boundarySourceLine.textLength,
                lineText,
            });
        } else {
            existingIdentifiers.insert(identifier.toLower());
        }
        areaReferences.append(identifier);
    }

    if (areaReferences.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "Smart Area has no writable boundary references.");
        }
        return false;
    }

    QStringList areaLines;
    areaLines.append(QStringLiteral("  %1")
                         .arg(draftObjectHeader(QStringLiteral("area"), QStringLiteral("water"), objectOptions)));
    for (const QString &identifier : std::as_const(areaReferences)) {
        areaLines.append(QStringLiteral("    %1").arg(identifier));
    }
    areaLines.append(QStringLiteral("  endarea"));

    QString lineEnding;
    if (insertionIndex > 0 && insertionIndex - 1 < sourceDocument.lines.size()) {
        lineEnding = sourceDocument.lines.at(insertionIndex - 1).lineEnding;
    }
    if (lineEnding.isEmpty() && insertionIndex < sourceDocument.lines.size()) {
        lineEnding = sourceDocument.lines.at(insertionIndex).lineEnding;
    }
    if (lineEnding.isEmpty()) {
        lineEnding = TherionSourceText::detectedLineEnding(contents);
    }

    QString insertedText;
    for (const QString &line : std::as_const(areaLines)) {
        insertedText += line;
        insertedText += lineEnding;
    }

    const int insertOffset = insertionIndex < sourceDocument.lines.size()
        ? sourceDocument.lines.at(insertionIndex).startOffset
        : contents.size();
    edits->append(TherionSourceTextEdit{
        insertOffset,
        0,
        insertedText,
    });

    if (insertedLineNumber != nullptr) {
        *insertedLineNumber = insertionIndex + 1;
    }
    return true;
}

bool TherionDocumentEditor::pointCoordinateRewriteEdits(const QString &contents,
                                                        int lineNumber,
                                                        const QPointF &point,
                                                        QVector<TherionSourceTextEdit> *edits,
                                                        QString *errorMessage)
{
    if (edits == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "No edit target is available.");
        }
        return false;
    }

    edits->clear();

    if (lineNumber <= 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "The selected line number is invalid.");
        }
        return false;
    }

    const TherionSourceDocument sourceDocument = TherionSourceDocument::fromText(contents);
    const TherionSourceDocumentLine *sourceDocumentLine = sourceDocument.lineAtLineNumber(lineNumber);
    if (sourceDocumentLine == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "The selected line no longer exists.");
        }
        return false;
    }

    const TherionParsedSourceLine &sourceLine = sourceDocumentLine->sourceLine;
    const QString &lineText = sourceLine.text;
    const TherionParsedLine &parsedLine = sourceLine.parsed;
    if (parsedLine.directive != QStringLiteral("point") && parsedLine.directive != QStringLiteral("station")) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "The selected line is not a writable point geometry.");
        }
        return false;
    }

    const QPair<int, int> tokenPair = coordinateTokenPair(parsedLine);
    if (tokenPair.first < 0 || tokenPair.second < 0
        || tokenPair.first >= parsedLine.tokenSpans.size()
        || tokenPair.second >= parsedLine.tokenSpans.size()) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "The selected point line does not contain writable coordinates.");
        }
        return false;
    }

    const TherionParsedToken secondToken = parsedLine.tokenSpans.at(tokenPair.second);
    const TherionParsedToken firstToken = parsedLine.tokenSpans.at(tokenPair.first);
    if (firstToken.start < 0
        || firstToken.length < 0
        || firstToken.start + firstToken.length > lineText.size()
        || secondToken.start < 0
        || secondToken.length < 0
        || secondToken.start + secondToken.length > lineText.size()) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "The selected point coordinates could not be rewritten.");
        }
        return false;
    }

    const QString oldYTokenText = lineText.mid(secondToken.start, secondToken.length);
    const QString oldXTokenText = lineText.mid(firstToken.start, firstToken.length);
    edits->append(TherionSourceTextEdit{
        sourceLine.absoluteTokenStart(firstToken),
        firstToken.length,
        formatCoordinateLikeExistingToken(oldXTokenText, point.x()),
    });
    edits->append(TherionSourceTextEdit{
        sourceLine.absoluteTokenStart(secondToken),
        secondToken.length,
        formatCoordinateLikeExistingToken(oldYTokenText, point.y()),
    });
    return true;
}

bool TherionDocumentEditor::lineAreaVertexRewriteEdits(const QString &contents,
                                                       int lineNumber,
                                                       const QString &kind,
                                                       int vertexIndex,
                                                       const QPointF &point,
                                                       QVector<TherionSourceTextEdit> *edits,
                                                       QString *errorMessage)
{
    if (edits == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "No source edit output is available.");
        }
        return false;
    }

    if (lineNumber <= 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "The selected line number is invalid.");
        }
        return false;
    }

    if (vertexIndex < 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "The selected vertex index is invalid.");
        }
        return false;
    }

    const QString normalizedKind = kind.trimmed().toLower();
    if (normalizedKind != QStringLiteral("line") && normalizedKind != QStringLiteral("area")) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "The selected geometry kind is not writable.");
        }
        return false;
    }

    const TherionSourceDocument sourceDocument = TherionSourceDocument::fromText(contents);
    const TherionParsedSourceDocument &parsedDocument = sourceDocument.parsedDocument();
    QStringList lines;
    lines.reserve(parsedDocument.lines.size());
    for (const TherionParsedSourceLine &sourceLine : parsedDocument.lines) {
        lines.append(sourceLine.text);
    }
    const TherionSourceDocumentLine *blockStartSourceLine = sourceDocument.lineAtLineNumber(lineNumber);
    if (blockStartSourceLine == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "The selected line no longer exists.");
        }
        return false;
    }

    const int blockStartLineIndex = lineNumber - 1;
    const TherionParsedLine startLine = TherionDocumentParser::parseLine(lines.at(blockStartLineIndex), lineNumber);
    if (startLine.directive != normalizedKind) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "The selected line is not a writable %1 geometry block.").arg(normalizedKind);
        }
        return false;
    }

    const QString blockEndDirective = normalizedKind == QStringLiteral("line")
        ? QStringLiteral("endline")
        : QStringLiteral("endarea");
    int blockEndLineIndex = -1;
    for (int candidateIndex = blockStartLineIndex + 1; candidateIndex < lines.size(); ++candidateIndex) {
        const TherionParsedLine candidateLine = TherionDocumentParser::parseLine(lines.at(candidateIndex), candidateIndex + 1);
        if (candidateLine.directive == blockEndDirective) {
            blockEndLineIndex = candidateIndex;
            break;
        }
    }

    if (blockEndLineIndex < 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "The selected %1 block is missing %2.").arg(normalizedKind, blockEndDirective);
        }
        return false;
    }

    struct CoordinateTokenReference
    {
        int lineIndex = -1;
        TherionParsedToken xToken;
        TherionParsedToken yToken;
    };

    QVector<CoordinateTokenReference> references;
    for (int lineIndex = blockStartLineIndex; lineIndex < blockEndLineIndex; ++lineIndex) {
        const TherionParsedLine parsedLine = TherionDocumentParser::parseLine(lines.at(lineIndex), lineIndex + 1);
        const int startTokenIndex = lineIndex == blockStartLineIndex ? 1 : 0;
        const QVector<QPair<int, int>> pairs = coordinateTokenPairsForLine(parsedLine, startTokenIndex);
        for (const QPair<int, int> &pair : pairs) {
            if (pair.first < 0 || pair.second < 0
                || pair.first >= parsedLine.tokenSpans.size()
                || pair.second >= parsedLine.tokenSpans.size()) {
                continue;
            }

            CoordinateTokenReference reference;
            reference.lineIndex = lineIndex;
            reference.xToken = parsedLine.tokenSpans.at(pair.first);
            reference.yToken = parsedLine.tokenSpans.at(pair.second);
            references.append(reference);
        }
    }

    if (vertexIndex >= references.size()) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "The selected %1 block does not contain a writable vertex at index %2.")
                                .arg(normalizedKind)
                                .arg(vertexIndex);
        }
        return false;
    }

    const CoordinateTokenReference reference = references.at(vertexIndex);
    if (reference.lineIndex < 0 || reference.lineIndex >= lines.size()) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "The selected %1 vertex could not be rewritten.").arg(normalizedKind);
        }
        return false;
    }

    QString lineText = lines.at(reference.lineIndex);
    if (reference.xToken.start < 0 || reference.yToken.start < 0
        || reference.xToken.length < 0 || reference.yToken.length < 0
        || reference.xToken.start + reference.xToken.length > lineText.size()
        || reference.yToken.start + reference.yToken.length > lineText.size()) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "The selected %1 vertex coordinates could not be rewritten.").arg(normalizedKind);
        }
        return false;
    }

    const QString oldYTokenText = lineText.mid(reference.yToken.start, reference.yToken.length);
    const QString oldXTokenText = lineText.mid(reference.xToken.start, reference.xToken.length);
    lineText.replace(reference.yToken.start,
                     reference.yToken.length,
                     formatCoordinateLikeExistingToken(oldYTokenText, point.y()));
    lineText.replace(reference.xToken.start,
                     reference.xToken.length,
                     formatCoordinateLikeExistingToken(oldXTokenText, point.x()));
    if (reference.lineIndex >= parsedDocument.lines.size()) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "The selected %1 vertex source range could not be rewritten.").arg(normalizedKind);
        }
        return false;
    }

    const TherionParsedSourceLine &sourceLine = parsedDocument.lines.at(reference.lineIndex);
    edits->append(TherionSourceTextEdit{
        sourceLine.startOffset,
        sourceLine.textLength,
        lineText,
    });
    return true;
}

bool TherionDocumentEditor::lineOptionToggleRewriteEdits(const QString &contents,
                                                         int lineNumber,
                                                         const QString &optionName,
                                                         bool enabled,
                                                         QVector<TherionSourceTextEdit> *edits,
                                                         QString *errorMessage)
{
    if (edits == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "No edit target is available.");
        }
        return false;
    }

    edits->clear();

    if (lineNumber <= 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "The selected line number is invalid.");
        }
        return false;
    }

    const std::optional<QString> normalizedOption = normalizedLineToggleOptionName(optionName);
    if (!normalizedOption.has_value()) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "Unsupported line option toggle.");
        }
        return false;
    }

    const TherionSourceDocument sourceDocument = TherionSourceDocument::fromText(contents);
    const TherionSourceDocumentLine *sourceDocumentLine = sourceDocument.lineAtLineNumber(lineNumber);
    if (sourceDocumentLine == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "The selected line no longer exists.");
        }
        return false;
    }

    const TherionParsedSourceLine &sourceLine = sourceDocumentLine->sourceLine;
    QString lineText = sourceLine.text;
    const TherionParsedLine &parsedLine = sourceLine.parsed;
    if (parsedLine.directive != QStringLiteral("line")) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "The selected line is not a writable line directive.");
        }
        return false;
    }

    int optionTokenIndex = -1;
    int valueTokenIndex = -1;
    for (int index = 1; index < parsedLine.tokens.size(); ++index) {
        if (parsedLine.tokens.at(index).toLower() != normalizedOption.value()) {
            continue;
        }

        optionTokenIndex = index;
        valueTokenIndex = -1;
        if (index + 1 < parsedLine.tokens.size()) {
            const QString nextToken = parsedLine.tokens.at(index + 1);
            if (!nextToken.startsWith(QLatin1Char('-'))) {
                valueTokenIndex = index + 1;
            }
        }
    }

    const QString optionValue = enabled ? QStringLiteral("on") : QStringLiteral("off");
    if (optionTokenIndex >= 0) {
        if (valueTokenIndex >= 0 && valueTokenIndex < parsedLine.tokenSpans.size()) {
            const TherionParsedToken valueToken = parsedLine.tokenSpans.at(valueTokenIndex);
            if (valueToken.start < 0
                || valueToken.length < 0
                || valueToken.start + valueToken.length > lineText.size()) {
                if (errorMessage != nullptr) {
                    *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "The selected line option value could not be rewritten.");
                }
                return false;
            }

            lineText.replace(valueToken.start, valueToken.length, optionValue);
        } else {
            if (optionTokenIndex >= parsedLine.tokenSpans.size()) {
                if (errorMessage != nullptr) {
                    *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "The selected line option could not be rewritten.");
                }
                return false;
            }

            const TherionParsedToken optionToken = parsedLine.tokenSpans.at(optionTokenIndex);
            if (optionToken.start < 0
                || optionToken.length < 0
                || optionToken.start + optionToken.length > lineText.size()) {
                if (errorMessage != nullptr) {
                    *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "The selected line option could not be rewritten.");
                }
                return false;
            }

            lineText.insert(optionToken.start + optionToken.length, QStringLiteral(" %1").arg(optionValue));
        }
    } else {
        const int insertionIndex = parsedLine.commentStart >= 0 ? parsedLine.commentStart : lineText.size();
        const bool needsLeadingSpace = insertionIndex > 0 && !lineText.at(insertionIndex - 1).isSpace();
        const bool needsTrailingSpace = insertionIndex < lineText.size() && !lineText.at(insertionIndex).isSpace();
        QString insertionText = QStringLiteral("%1 %2").arg(normalizedOption.value(), optionValue);
        if (needsLeadingSpace) {
            insertionText.prepend(QLatin1Char(' '));
        }
        if (needsTrailingSpace) {
            insertionText.append(QLatin1Char(' '));
        }
        lineText.insert(insertionIndex, insertionText);
    }

    if (lineText != sourceLine.text) {
        edits->append(TherionSourceTextEdit{
            sourceLine.startOffset,
            sourceLine.textLength,
            lineText,
        });
    }
    return true;
}

bool TherionDocumentEditor::pointOrientationRewriteEdits(const QString &contents,
                                                         int lineNumber,
                                                         bool enabled,
                                                         qreal orientationDegrees,
                                                         QVector<TherionSourceTextEdit> *edits,
                                                         QString *errorMessage)
{
    if (edits == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "No edit target is available.");
        }
        return false;
    }

    edits->clear();

    if (lineNumber <= 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "The selected line number is invalid.");
        }
        return false;
    }

    const TherionSourceDocument sourceDocument = TherionSourceDocument::fromText(contents);
    const TherionSourceDocumentLine *sourceDocumentLine = sourceDocument.lineAtLineNumber(lineNumber);
    if (sourceDocumentLine == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "The selected line no longer exists.");
        }
        return false;
    }

    const TherionParsedSourceLine &sourceLine = sourceDocumentLine->sourceLine;
    QString lineText = sourceLine.text;
    const TherionParsedLine &parsedLine = sourceLine.parsed;
    if (parsedLine.directive != QStringLiteral("point")
        && parsedLine.directive != QStringLiteral("station")) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "The selected line is not a writable point command.");
        }
        return false;
    }

    int optionTokenIndex = -1;
    int valueTokenIndex = -1;
    for (int index = 1; index < parsedLine.tokens.size(); ++index) {
        if (!isOrientationOptionToken(parsedLine.tokens.at(index))) {
            continue;
        }
        optionTokenIndex = index;
        valueTokenIndex = -1;
        if (index + 1 < parsedLine.tokens.size()) {
            const QString nextToken = parsedLine.tokens.at(index + 1).trimmed();
            if (!nextToken.startsWith(QLatin1Char('-'))) {
                valueTokenIndex = index + 1;
            }
        }
    }

    if (!enabled) {
        if (optionTokenIndex < 0) {
            return true;
        }
        if (!removeOptionAtTokenIndex(&lineText, parsedLine, optionTokenIndex)) {
            if (errorMessage != nullptr) {
                *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "Point orientation could not be removed.");
            }
            return false;
        }
        if (lineText != sourceLine.text) {
            edits->append(TherionSourceTextEdit{
                sourceLine.startOffset,
                sourceLine.textLength,
                lineText,
            });
        }
        return true;
    }

    const QString optionValue = formatOrientationDegrees(orientationDegrees);
    if (optionTokenIndex >= 0) {
        if (valueTokenIndex >= 0 && valueTokenIndex < parsedLine.tokenSpans.size()) {
            const TherionParsedToken valueToken = parsedLine.tokenSpans.at(valueTokenIndex);
            if (valueToken.start < 0
                || valueToken.length < 0
                || valueToken.start + valueToken.length > lineText.size()) {
                if (errorMessage != nullptr) {
                    *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "Point orientation value could not be rewritten.");
                }
                return false;
            }
            lineText.replace(valueToken.start, valueToken.length, optionValue);
        } else {
            if (optionTokenIndex >= parsedLine.tokenSpans.size()) {
                if (errorMessage != nullptr) {
                    *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "Point orientation option could not be rewritten.");
                }
                return false;
            }
            const TherionParsedToken optionToken = parsedLine.tokenSpans.at(optionTokenIndex);
            if (optionToken.start < 0
                || optionToken.length < 0
                || optionToken.start + optionToken.length > lineText.size()) {
                if (errorMessage != nullptr) {
                    *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "Point orientation option could not be rewritten.");
                }
                return false;
            }
            lineText.insert(optionToken.start + optionToken.length,
                            QStringLiteral(" %1").arg(optionValue));
        }
    } else {
        const int insertionIndex = parsedLine.commentStart >= 0 ? parsedLine.commentStart : lineText.size();
        const bool needsLeadingSpace = insertionIndex > 0 && !lineText.at(insertionIndex - 1).isSpace();
        const bool needsTrailingSpace = insertionIndex < lineText.size() && !lineText.at(insertionIndex).isSpace();
        QString insertionText = QStringLiteral("-orientation %1").arg(optionValue);
        if (needsLeadingSpace) {
            insertionText.prepend(QLatin1Char(' '));
        }
        if (needsTrailingSpace) {
            insertionText.append(QLatin1Char(' '));
        }
        lineText.insert(insertionIndex, insertionText);
    }

    if (lineText != sourceLine.text) {
        edits->append(TherionSourceTextEdit{
            sourceLine.startOffset,
            sourceLine.textLength,
            lineText,
        });
    }
    return true;
}

bool TherionDocumentEditor::mapObjectClipDisabledRewriteEdits(const QString &contents,
                                                              int lineNumber,
                                                              bool disabled,
                                                              QVector<TherionSourceTextEdit> *edits,
                                                              QString *errorMessage)
{
    if (edits == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "No edit target is available.");
        }
        return false;
    }

    edits->clear();

    if (lineNumber <= 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "The selected line number is invalid.");
        }
        return false;
    }

    const TherionSourceDocument sourceDocument = TherionSourceDocument::fromText(contents);
    const TherionSourceDocumentLine *sourceDocumentLine = sourceDocument.lineAtLineNumber(lineNumber);
    if (sourceDocumentLine == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "The selected line no longer exists.");
        }
        return false;
    }

    const TherionParsedSourceLine &sourceLine = sourceDocumentLine->sourceLine;
    QString lineText = sourceLine.text;
    const TherionParsedLine &parsedLine = sourceLine.parsed;
    if (parsedLine.directive != QStringLiteral("line")
        && parsedLine.directive != QStringLiteral("area")
        && parsedLine.directive != QStringLiteral("point")) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "Clip is available only for map point, line, and area commands.");
        }
        return false;
    }

    if (!upsertSingleValueOption(&lineText,
                                 QStringLiteral("-clip"),
                                 disabled ? QStringLiteral("off") : QString())) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "The selected object clip option could not be rewritten.");
        }
        return false;
    }

    if (lineText != sourceLine.text) {
        edits->append(TherionSourceTextEdit{
            sourceLine.startOffset,
            sourceLine.textLength,
            lineText,
        });
    }
    return true;
}

bool TherionDocumentEditor::pointAlignRewriteEdits(const QString &contents,
                                                   int lineNumber,
                                                   const QString &align,
                                                   QVector<TherionSourceTextEdit> *edits,
                                                   QString *errorMessage)
{
    if (edits == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "No edit target is available.");
        }
        return false;
    }

    edits->clear();

    if (lineNumber <= 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "The selected line number is invalid.");
        }
        return false;
    }

    const TherionParsedSourceDocument sourceDocument = TherionDocumentParser::parseSourceDocument(contents);
    if (lineNumber > sourceDocument.lines.size()) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "The selected line no longer exists.");
        }
        return false;
    }

    const TherionParsedSourceLine &sourceLine = sourceDocument.lines.at(lineNumber - 1);
    QString lineText = sourceLine.text;
    const TherionParsedLine &parsedLine = sourceLine.parsed;
    if (parsedLine.directive != QStringLiteral("point")) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "Align is available only for point commands.");
        }
        return false;
    }

    if (!upsertSingleValueOption(&lineText, QStringLiteral("-align"), align.trimmed())) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "The selected point align option could not be rewritten.");
        }
        return false;
    }

    if (lineText != sourceLine.text) {
        edits->append(TherionSourceTextEdit{
            sourceLine.startOffset,
            sourceLine.textLength,
            lineText,
        });
    }
    return true;
}

bool TherionDocumentEditor::linePointOrientationRewriteEdits(const QString &contents,
                                                             int lineNumber,
                                                             int sourceVertexIndex,
                                                             bool enabled,
                                                             qreal orientationDegrees,
                                                             QVector<TherionSourceTextEdit> *edits,
                                                             QString *errorMessage)
{
    return linePointNumericOptionRewriteEdits(contents,
                                             lineNumber,
                                             sourceVertexIndex,
                                             QStringLiteral("-orientation"),
                                             enabled,
                                             orientationDegrees,
                                             edits,
                                             errorMessage);
}

bool TherionDocumentEditor::linePointLeftSizeRewriteEdits(const QString &contents,
                                                          int lineNumber,
                                                          int sourceVertexIndex,
                                                          bool enabled,
                                                          qreal sizeValue,
                                                          QVector<TherionSourceTextEdit> *edits,
                                                          QString *errorMessage)
{
    return linePointNumericOptionRewriteEdits(contents,
                                             lineNumber,
                                             sourceVertexIndex,
                                             QStringLiteral("-l-size"),
                                             enabled,
                                             sizeValue,
                                             edits,
                                             errorMessage);
}

bool TherionDocumentEditor::scrapScaleRewriteEdits(const QString &contents,
                                                   int lineNumber,
                                                   const QString &scaleExpression,
                                                   QVector<TherionSourceTextEdit> *edits,
                                                   QString *errorMessage)
{
    if (edits == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "No edit target is available.");
        }
        return false;
    }

    edits->clear();

    if (lineNumber <= 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "The selected line number is invalid.");
        }
        return false;
    }

    const QString normalizedScale = scaleExpression.trimmed();
    if (normalizedScale.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "The scrap scale expression is empty.");
        }
        return false;
    }

    const TherionParsedSourceDocument sourceDocument = TherionDocumentParser::parseSourceDocument(contents);
    if (lineNumber > sourceDocument.lines.size()) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "The selected line no longer exists.");
        }
        return false;
    }

    const TherionParsedSourceLine &sourceLine = sourceDocument.lines.at(lineNumber - 1);
    QString lineText = sourceLine.text;
    const TherionParsedLine &parsedLine = sourceLine.parsed;
    if (parsedLine.directive != QStringLiteral("scrap")) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "The selected line is not a scrap command.");
        }
        return false;
    }

    const QString replacement = QStringLiteral("-scale %1").arg(normalizedScale);
    const int scaleOptionIndex = optionTokenIndex(parsedLine, QStringLiteral("-scale"));
    if (scaleOptionIndex >= 0) {
        const QPair<int, int> range = optionRangeWithBracketedValue(parsedLine, scaleOptionIndex, lineText);
        if (range.first < 0 || range.second < range.first || range.second > lineText.size()) {
            if (errorMessage != nullptr) {
                *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "The existing scrap scale option could not be rewritten.");
            }
            return false;
        }

        const bool includesLeadingSpace = range.first < parsedLine.tokenSpans.at(scaleOptionIndex).start;
        lineText.replace(range.first,
                         range.second - range.first,
                         includesLeadingSpace ? QStringLiteral(" ") + replacement : replacement);
    } else {
        if (parsedLine.commentStart >= 0) {
            int insertIndex = parsedLine.commentStart;
            while (insertIndex > 0 && lineText.at(insertIndex - 1).isSpace()) {
                --insertIndex;
            }
            lineText.insert(insertIndex, QStringLiteral(" ") + replacement);
        } else {
            lineText += QStringLiteral(" ") + replacement;
        }
    }

    if (lineText != sourceLine.text) {
        edits->append(TherionSourceTextEdit{
            sourceLine.startOffset,
            sourceLine.textLength,
            lineText,
        });
    }
    return true;
}

bool TherionDocumentEditor::scrapProjectionRewriteEdits(const QString &contents,
                                                        int lineNumber,
                                                        const QString &projectionExpression,
                                                        QVector<TherionSourceTextEdit> *edits,
                                                        QString *errorMessage)
{
    if (edits == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "No edit target is available.");
        }
        return false;
    }

    edits->clear();

    if (lineNumber <= 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "The selected line number is invalid.");
        }
        return false;
    }

    const TherionParsedSourceDocument sourceDocument = TherionDocumentParser::parseSourceDocument(contents);
    if (lineNumber > sourceDocument.lines.size()) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "The selected line no longer exists.");
        }
        return false;
    }

    const TherionParsedSourceLine &sourceLine = sourceDocument.lines.at(lineNumber - 1);
    QString lineText = sourceLine.text;
    const TherionParsedLine &parsedLine = sourceLine.parsed;
    if (parsedLine.directive != QStringLiteral("scrap")) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "The selected line is not a scrap command.");
        }
        return false;
    }

    const int projectionOptionIndex = optionTokenIndex(parsedLine, QStringLiteral("-projection"));
    const QString normalizedProjection = projectionExpression.trimmed();
    if (projectionOptionIndex >= 0) {
        const QPair<int, int> range = optionRangeWithBracketedValue(parsedLine, projectionOptionIndex, lineText);
        if (range.first < 0 || range.second < range.first || range.second > lineText.size()) {
            if (errorMessage != nullptr) {
                *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "The existing scrap projection option could not be rewritten.");
            }
            return false;
        }

        if (normalizedProjection.isEmpty()) {
            lineText.remove(range.first, range.second - range.first);
        } else {
            const bool includesLeadingSpace = range.first < parsedLine.tokenSpans.at(projectionOptionIndex).start;
            const QString replacement = QStringLiteral("-projection %1").arg(normalizedProjection);
            lineText.replace(range.first,
                             range.second - range.first,
                             includesLeadingSpace ? QStringLiteral(" ") + replacement : replacement);
        }
    } else {
        if (normalizedProjection.isEmpty()) {
            return true;
        }
        const QString insertionText = QStringLiteral("-projection %1").arg(normalizedProjection);
        if (parsedLine.commentStart >= 0) {
            int insertIndex = parsedLine.commentStart;
            while (insertIndex > 0 && lineText.at(insertIndex - 1).isSpace()) {
                --insertIndex;
            }
            lineText.insert(insertIndex, QStringLiteral(" ") + insertionText);
        } else {
            lineText += QStringLiteral(" ") + insertionText;
        }
    }

    if (lineText != sourceLine.text) {
        edits->append(TherionSourceTextEdit{
            sourceLine.startOffset,
            sourceLine.textLength,
            lineText,
        });
    }
    return true;
}

bool TherionDocumentEditor::mapObjectQuickFieldsRewriteEdits(const QString &contents,
                                                             int lineNumber,
                                                             const QString &type,
                                                             const QString &subtype,
                                                             const QString &identifier,
                                                             const QString &name,
                                                             bool nameEnabled,
                                                             QVector<TherionSourceTextEdit> *edits,
                                                             QString *errorMessage)
{
    if (edits == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "No edit target is available.");
        }
        return false;
    }

    edits->clear();

    if (lineNumber <= 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "The selected line number is invalid.");
        }
        return false;
    }

    const TherionParsedSourceDocument sourceDocument = TherionDocumentParser::parseSourceDocument(contents);
    if (lineNumber > sourceDocument.lines.size()) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "The selected line no longer exists.");
        }
        return false;
    }

    const TherionParsedSourceLine &sourceLine = sourceDocument.lines.at(lineNumber - 1);
    QString lineText = sourceLine.text;
    TherionParsedLine parsedLine = sourceLine.parsed;
    if (parsedLine.directive.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "The selected line is not a map object command.");
        }
        return false;
    }

    const QString directive = parsedLine.directive;
    if (directive == QStringLiteral("scrap")) {
        const QString normalizedIdentifier = identifier.trimmed();
        if (normalizedIdentifier.isEmpty()) {
            if (errorMessage != nullptr) {
                *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "Scrap ID cannot be empty.");
            }
            return false;
        }
        if (parsedLine.tokens.size() < 2 || parsedLine.tokenSpans.size() < 2) {
            if (errorMessage != nullptr) {
                *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "The selected scrap command has no writable ID token.");
            }
            return false;
        }
        const QString replacement = replacementTokenForLine(normalizedIdentifier, lineText, parsedLine.tokenSpans.at(1));
        if (!replaceTokenText(&lineText, parsedLine, 1, replacement)) {
            if (errorMessage != nullptr) {
                *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "The selected scrap ID could not be rewritten.");
            }
            return false;
        }
        if (lineText != sourceLine.text) {
            edits->append(TherionSourceTextEdit{
                sourceLine.startOffset,
                sourceLine.textLength,
                lineText,
            });
        }
        return true;
    }

    if (directive != QStringLiteral("point")
        && directive != QStringLiteral("line")
        && directive != QStringLiteral("area")) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "Quick object fields are available only for scrap, point, line, and area commands.");
        }
        return false;
    }

    const QString normalizedType = type.trimmed();
    if (normalizedType.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "Object type cannot be empty.");
        }
        return false;
    }

    const int typeTokenIndex = directive == QStringLiteral("point")
        ? pointTypeTokenIndex(parsedLine)
        : 1;
    if (directive == QStringLiteral("point") && typeTokenIndex < 0) {
        const QPair<int, int> coordinatePair = coordinateTokenPair(parsedLine);
        const int insertAfterTokenIndex = coordinatePair.second;
        if (insertAfterTokenIndex < 0 || insertAfterTokenIndex >= parsedLine.tokenSpans.size()) {
            if (errorMessage != nullptr) {
                *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "The selected point command has no writable coordinate pair for inserting type.");
            }
            return false;
        }
        const TherionParsedToken coordinateToken = parsedLine.tokenSpans.at(insertAfterTokenIndex);
        if (coordinateToken.start < 0
            || coordinateToken.length < 0
            || coordinateToken.start + coordinateToken.length > lineText.size()) {
            if (errorMessage != nullptr) {
                *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "The selected point type insertion target is invalid.");
            }
            return false;
        }
        lineText.insert(coordinateToken.start + coordinateToken.length,
                        QStringLiteral(" ") + serializedInlineToken(normalizedType));
    } else if (typeTokenIndex <= 0 || typeTokenIndex >= parsedLine.tokenSpans.size()) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "The selected %1 command has no writable type token.").arg(directive);
        }
        return false;
    } else if (!replaceTokenText(&lineText, parsedLine, typeTokenIndex, serializedInlineToken(normalizedType))) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "The selected object type could not be rewritten.");
        }
        return false;
    }

    const QString normalizedSubtype = subtype.trimmed();
    const QString subtypeForWrite = TherionTokenRules::tokenStartsOption(normalizedSubtype) ? QString() : normalizedSubtype;
    if (!upsertSingleValueOption(&lineText, QStringLiteral("-subtype"), subtypeForWrite)) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "The selected object subtype could not be rewritten.");
        }
        return false;
    }

    if ((directive == QStringLiteral("line") || directive == QStringLiteral("area"))
        && !removeMalformedMapObjectOptionTokens(&lineText)) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "Malformed option-like object tokens could not be removed.");
        }
        return false;
    }

    if (!upsertSingleValueOption(&lineText, QStringLiteral("-id"), identifier)) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "The selected object identifier could not be rewritten.");
        }
        return false;
    }

    if (directive == QStringLiteral("point")
        && nameEnabled
        && !upsertSingleValueOption(&lineText, QStringLiteral("-name"), name)) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "The selected point name could not be rewritten.");
        }
        return false;
    }

    if (lineText != sourceLine.text) {
        edits->append(TherionSourceTextEdit{
            sourceLine.startOffset,
            sourceLine.textLength,
            lineText,
        });
    }
    return true;
}

bool TherionDocumentEditor::mapObjectTextOptionRewriteEdits(const QString &contents,
                                                            int lineNumber,
                                                            const QString &text,
                                                            QVector<TherionSourceTextEdit> *edits,
                                                            QString *errorMessage)
{
    if (edits == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "No edit target is available.");
        }
        return false;
    }

    edits->clear();

    if (lineNumber <= 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "The selected line number is invalid.");
        }
        return false;
    }

    const TherionParsedSourceDocument sourceDocument = TherionDocumentParser::parseSourceDocument(contents);
    if (lineNumber > sourceDocument.lines.size()) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "The selected line no longer exists.");
        }
        return false;
    }

    const TherionParsedSourceLine &sourceLine = sourceDocument.lines.at(lineNumber - 1);
    QString lineText = sourceLine.text;
    const TherionParsedLine &parsedLine = sourceLine.parsed;
    const QString directive = parsedLine.directive;
    if (directive != QStringLiteral("point") && directive != QStringLiteral("line")) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "Text is available only for point and line label commands.");
        }
        return false;
    }

    const int typeTokenIndex = directive == QStringLiteral("point") ? pointTypeTokenIndex(parsedLine) : 1;
    if (typeTokenIndex <= 0 || typeTokenIndex >= parsedLine.tokens.size()) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "The selected object has no writable type token.");
        }
        return false;
    }

    const QString normalizedType = parsedLine.tokens.at(typeTokenIndex)
        .section(QLatin1Char(':'), 0, 0)
        .trimmed()
        .toLower();
    if (normalizedType != QStringLiteral("label") && !text.trimmed().isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "Text is available only for label objects.");
        }
        return false;
    }

    if (!upsertSingleValueOption(&lineText, QStringLiteral("-text"), text)) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "The selected label text could not be rewritten.");
        }
        return false;
    }

    if (lineText != sourceLine.text) {
        edits->append(TherionSourceTextEdit{
            sourceLine.startOffset,
            sourceLine.textLength,
            lineText,
        });
    }
    return true;
}

bool TherionDocumentEditor::mapObjectValueOptionRewriteEdits(const QString &contents,
                                                             int lineNumber,
                                                             const QString &value,
                                                             QVector<TherionSourceTextEdit> *edits,
                                                             QString *errorMessage)
{
    if (edits == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "No edit target is available.");
        }
        return false;
    }

    edits->clear();

    if (lineNumber <= 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "The selected line number is invalid.");
        }
        return false;
    }

    const TherionParsedSourceDocument sourceDocument = TherionDocumentParser::parseSourceDocument(contents);
    if (lineNumber > sourceDocument.lines.size()) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "The selected line no longer exists.");
        }
        return false;
    }

    const TherionParsedSourceLine &sourceLine = sourceDocument.lines.at(lineNumber - 1);
    QString lineText = sourceLine.text;
    const TherionParsedLine &parsedLine = sourceLine.parsed;
    if (parsedLine.directive != QStringLiteral("point")) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "Value is available only for supported point commands.");
        }
        return false;
    }

    const int typeTokenIndex = pointTypeTokenIndex(parsedLine);
    if (typeTokenIndex <= 0 || typeTokenIndex >= parsedLine.tokens.size()) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "The selected point has no writable type token.");
        }
        return false;
    }

    const QString normalizedType = parsedLine.tokens.at(typeTokenIndex)
        .section(QLatin1Char(':'), 0, 0)
        .trimmed()
        .toLower();
    if (!pointTypeSupportsValueOption(normalizedType) && !value.trimmed().isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "Value is available only for supported point types.");
        }
        return false;
    }

    if (!upsertMapObjectValueOption(&lineText, value)) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "The selected point value could not be rewritten.");
        }
        return false;
    }

    if (lineText != sourceLine.text) {
        edits->append(TherionSourceTextEdit{
            sourceLine.startOffset,
            sourceLine.textLength,
            lineText,
        });
    }
    return true;
}

bool TherionDocumentEditor::lineCoordinateRowsRewriteEdits(const QString &contents,
                                                           int lineNumber,
                                                           const QStringList &coordinateRows,
                                                           QVector<TherionSourceTextEdit> *edits,
                                                           QString *errorMessage)
{
    if (edits == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "No source edit output is available.");
        }
        return false;
    }
    if (lineNumber <= 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "The selected line number is invalid.");
        }
        return false;
    }
    if (coordinateRows.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "No coordinate rows were provided for rewrite.");
        }
        return false;
    }

    const TherionParsedSourceDocument sourceDocument = TherionDocumentParser::parseSourceDocument(contents);
    QStringList lines;
    lines.reserve(sourceDocument.lines.size());
    for (const TherionParsedSourceLine &sourceLine : sourceDocument.lines) {
        lines.append(sourceLine.text);
    }
    if (lineNumber > lines.size()) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "The selected line no longer exists.");
        }
        return false;
    }

    const int blockStartLineIndex = lineNumber - 1;
    const TherionParsedLine startLine = TherionDocumentParser::parseLine(lines.at(blockStartLineIndex), lineNumber);
    if (startLine.directive != QStringLiteral("line")) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "The selected line is not a writable line block.");
        }
        return false;
    }

    if (!coordinateTokenPairsForLine(startLine, 1).isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "Line rewrite is not supported when start line contains inline coordinates.");
        }
        return false;
    }

    int blockEndLineIndex = -1;
    for (int candidateIndex = blockStartLineIndex + 1; candidateIndex < lines.size(); ++candidateIndex) {
        const TherionParsedLine candidateLine = TherionDocumentParser::parseLine(lines.at(candidateIndex), candidateIndex + 1);
        if (candidateLine.directive == QStringLiteral("endline")) {
            blockEndLineIndex = candidateIndex;
            break;
        }
    }
    if (blockEndLineIndex < 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "The selected line block is missing endline.");
        }
        return false;
    }

    for (int lineIndex = blockStartLineIndex + 1; lineIndex < blockEndLineIndex; ++lineIndex) {
        const TherionParsedLine parsedLine = TherionDocumentParser::parseLine(lines.at(lineIndex), lineIndex + 1);
        if (parsedLine.tokens.isEmpty()) {
            continue;
        }
        if (parsedLine.commentStart == 0) {
            continue;
        }
        if (!coordinateTokenPairsForLine(parsedLine, 0).isEmpty()) {
            continue;
        }
    }

    QStringList rewrittenBlock;
    rewrittenBlock.reserve(2 + coordinateRows.size());
    rewrittenBlock.append(lines.at(blockStartLineIndex));
    for (const QString &row : coordinateRows) {
        const QString trimmed = row.trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }
        rewrittenBlock.append(QStringLiteral("  %1").arg(trimmed));
    }
    rewrittenBlock.append(lines.at(blockEndLineIndex));

    if (blockStartLineIndex >= sourceDocument.lines.size() || blockEndLineIndex >= sourceDocument.lines.size()) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::TherionDocumentEditor", "The selected line block source range could not be rewritten.");
        }
        return false;
    }

    QString replacementLineEnding = sourceDocument.lines.at(blockStartLineIndex).lineEnding;
    if (replacementLineEnding.isEmpty()) {
        replacementLineEnding = TherionSourceText::detectedLineEnding(contents);
    }
    const QString replacementText = rewrittenBlock.join(replacementLineEnding);
    const TherionParsedSourceLine &startSourceLine = sourceDocument.lines.at(blockStartLineIndex);
    const TherionParsedSourceLine &endSourceLine = sourceDocument.lines.at(blockEndLineIndex);
    const int replaceStartOffset = startSourceLine.startOffset;
    const int replaceLength = (endSourceLine.startOffset + endSourceLine.textLength) - replaceStartOffset;
    edits->append(TherionSourceTextEdit{
        replaceStartOffset,
        replaceLength,
        replacementText,
    });
    return true;
}
}
