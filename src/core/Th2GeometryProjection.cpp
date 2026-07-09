#include "Th2GeometryProjection.h"

#include "TherionTokenRules.h"

#include <QRegularExpression>
#include <QUrl>

#include <utility>

namespace TherionStudio
{
namespace
{
QString normalizedOptionName(QString option)
{
    option = option.trimmed().toLower();
    while (option.startsWith(QLatin1Char('-'))) {
        option.remove(0, 1);
    }
    return option;
}

QString typePart(const QString &typeToken)
{
    return typeToken.section(QLatin1Char(':'), 0, 0).trimmed().toLower();
}

QString inlineSubtypePart(const QString &typeToken)
{
    return typeToken.section(QLatin1Char(':'), 1).trimmed().toLower();
}

QString objectKindToken(Th2GeometryObjectKind kind)
{
    switch (kind) {
    case Th2GeometryObjectKind::Point:
        return QStringLiteral("point");
    case Th2GeometryObjectKind::Line:
        return QStringLiteral("line");
    case Th2GeometryObjectKind::Area:
        return QStringLiteral("area");
    case Th2GeometryObjectKind::Scrap:
        return QStringLiteral("scrap");
    case Th2GeometryObjectKind::Map:
        return QStringLiteral("map");
    case Th2GeometryObjectKind::Background:
        return QStringLiteral("background");
    }
    return QStringLiteral("object");
}

QString stableKeyForCommand(const Th2CommandObject &object)
{
    const QString prefix = objectKindToken(object.kind);
    const QString explicitId = object.id.trimmed();
    if (!explicitId.isEmpty()) {
        return QStringLiteral("%1:id:%2").arg(prefix, explicitId);
    }
    return QStringLiteral("%1:line:%2:%3")
        .arg(prefix)
        .arg(object.sourceRange.startLineNumber)
        .arg(object.sourceRange.startOffset);
}

Th2SourceRange sourceRangeForLine(const TherionSourceDocumentLine &line)
{
    return {
        line.sourceLine.lineNumber,
        line.sourceLine.lineNumber,
        line.sourceLine.startOffset,
        line.sourceLine.endOffset,
    };
}

Th2SourceRange sourceRangeForCommand(const TherionSourceLogicalCommand &command)
{
    return {
        command.startLineNumber,
        command.endLineNumber,
        command.startOffset,
        command.endOffset,
    };
}

Th2SourceRange sourceRangeForBlock(const TherionSourceDocument &sourceDocument,
                                   const TherionSourceLogicalCommand &command)
{
    for (const TherionSourceBlockRange &range : sourceDocument.blockRanges()) {
        if (range.openLineNumber == command.startLineNumber
            && range.directive == command.normalizedDirective) {
            return {
                range.openLineNumber,
                range.closeLineNumber > 0 ? range.closeLineNumber : command.endLineNumber,
                range.startOffset,
                range.endOffset,
            };
        }
    }
    return sourceRangeForCommand(command);
}

QHash<QString, QStringList> optionsByName(const TherionSourceLogicalCommand &command)
{
    QHash<QString, QStringList> values;
    for (const TherionSourceLogicalOptionEntryRange &entry : command.optionEntryRanges) {
        const QString normalized = normalizedOptionName(entry.key);
        if (!normalized.isEmpty()) {
            values.insert(normalized, entry.rawValueTokens);
        }
    }
    return values;
}

QString optionValue(const QHash<QString, QStringList> &values, const QString &name)
{
    const QStringList tokens = values.value(normalizedOptionName(name));
    return tokens.join(QLatin1Char(' ')).trimmed();
}

Th2CommandObject commandObjectForLogicalCommand(Th2GeometryObjectKind kind,
                                                const TherionSourceDocument &sourceDocument,
                                                const TherionSourceLogicalCommand &command)
{
    Th2CommandObject object;
    object.kind = kind;
    object.directive = command.normalizedDirective;
    object.optionsByName = optionsByName(command);
    object.id = optionValue(object.optionsByName, QStringLiteral("id"));
    object.sourceRange = kind == Th2GeometryObjectKind::Line
            || kind == Th2GeometryObjectKind::Area
            || kind == Th2GeometryObjectKind::Scrap
            || kind == Th2GeometryObjectKind::Map
        ? sourceRangeForBlock(sourceDocument, command)
        : sourceRangeForCommand(command);
    object.stableKey = stableKeyForCommand(object);
    return object;
}

QString positionalToken(const TherionSourceLogicalCommand &command, int index)
{
    if (index < 0 || index >= command.positionalArgumentRanges.size()) {
        return QString();
    }
    return command.positionalArgumentRanges.at(index).text.trimmed();
}

void fillTypeAndSubtype(Th2CommandObject *object, const TherionSourceLogicalCommand &command, int typeArgumentIndex)
{
    if (object == nullptr) {
        return;
    }

    const QString typeToken = positionalToken(command, typeArgumentIndex);
    object->type = typePart(typeToken);
    object->subtype = optionValue(object->optionsByName, QStringLiteral("subtype")).toLower();
    if (object->subtype.isEmpty()) {
        object->subtype = inlineSubtypePart(typeToken);
    }
}

QVector<int> numericTokenIndexes(const TherionParsedLine &line)
{
    QVector<int> indexes;
    for (int index = 0; index < line.tokens.size(); ++index) {
        const QString token = line.tokens.at(index).trimmed();
        if (TherionTokenRules::tokenStartsOption(token)) {
            break;
        }
        if (TherionTokenRules::isNumericToken(token)) {
            indexes.append(index);
        } else if (!indexes.isEmpty()) {
            break;
        }
    }
    return indexes;
}

QVector<QPointF> coordinatePointsFromLine(const TherionParsedLine &line)
{
    const QVector<int> indexes = numericTokenIndexes(line);
    QVector<QPointF> points;
    points.reserve(indexes.size() / 2);
    for (int index = 0; index + 1 < indexes.size(); index += 2) {
        bool xOk = false;
        bool yOk = false;
        const qreal x = line.tokens.at(indexes.at(index)).toDouble(&xOk);
        const qreal y = line.tokens.at(indexes.at(index + 1)).toDouble(&yOk);
        if (xOk && yOk) {
            points.append(QPointF(x, y));
        }
    }
    return points;
}

QString pointTypeToken(const TherionSourceLogicalCommand &command)
{
    if (command.normalizedDirective == QStringLiteral("station")) {
        return QStringLiteral("station");
    }

    const QVector<int> numericIndexes = numericTokenIndexes(command.parsed);
    if (numericIndexes.size() < 2) {
        return QString();
    }

    const int afterSecondCoordinate = numericIndexes.at(1) + 1;
    for (int index = afterSecondCoordinate; index < command.parsed.tokens.size(); ++index) {
        const QString token = command.parsed.tokens.at(index).trimmed();
        if (token.isEmpty()) {
            continue;
        }
        if (TherionTokenRules::tokenStartsOption(token)) {
            break;
        }
        return token;
    }
    return QString();
}

QVector<Th2LinePointRow> lineRowsForBlock(const TherionSourceDocument &sourceDocument,
                                          const TherionSourceLogicalCommand &lineCommand)
{
    QVector<Th2LinePointRow> rows;
    const Th2SourceRange range = sourceRangeForBlock(sourceDocument, lineCommand);
    if (!range.isValid() || range.endLineNumber <= range.startLineNumber) {
        return rows;
    }

    for (int lineNumber = range.startLineNumber + 1; lineNumber < range.endLineNumber; ++lineNumber) {
        const TherionSourceDocumentLine *sourceLine = sourceDocument.lineAtLineNumber(lineNumber);
        if (sourceLine == nullptr || sourceLine->role != TherionSourceLineRole::BlockContent) {
            continue;
        }

        const TherionParsedLine &parsed = sourceLine->sourceLine.parsed;
        Th2LinePointRow row;
        row.lineNumber = lineNumber;
        row.text = sourceLine->sourceLine.text;
        row.sourceRange = sourceRangeForLine(*sourceLine);
        row.coordinatePoints = coordinatePointsFromLine(parsed);
        row.smoothOff = parsed.directive.compare(QStringLiteral("smooth"), Qt::CaseInsensitive) == 0
            && parsed.tokens.size() >= 2
            && parsed.tokens.at(1).compare(QStringLiteral("off"), Qt::CaseInsensitive) == 0;
        if (parsed.directive.compare(QStringLiteral("subtype"), Qt::CaseInsensitive) == 0
            && parsed.tokens.size() >= 2) {
            row.subtype = parsed.tokens.at(1).trimmed().toLower();
        }
        if (!row.coordinatePoints.isEmpty() || row.smoothOff || !row.subtype.isEmpty()) {
            rows.append(row);
        }
    }
    return rows;
}

QStringList areaBorderReferencesForBlock(const TherionSourceDocument &sourceDocument,
                                         const TherionSourceLogicalCommand &areaCommand)
{
    QStringList references;
    const Th2SourceRange range = sourceRangeForBlock(sourceDocument, areaCommand);
    if (!range.isValid() || range.endLineNumber <= range.startLineNumber) {
        return references;
    }

    for (int lineNumber = range.startLineNumber + 1; lineNumber < range.endLineNumber; ++lineNumber) {
        const TherionSourceDocumentLine *sourceLine = sourceDocument.lineAtLineNumber(lineNumber);
        if (sourceLine == nullptr || sourceLine->role != TherionSourceLineRole::BlockContent) {
            continue;
        }
        const TherionParsedLine &parsed = sourceLine->sourceLine.parsed;
        for (const QString &token : parsed.tokens) {
            if (TherionTokenRules::tokenStartsOption(token)) {
                break;
            }
            if (!TherionTokenRules::isNumericToken(token)) {
                references.append(token.trimmed());
            }
        }
    }
    references.removeAll(QString());
    return references;
}

QString xtherionImagePath(const QString &line)
{
    const int marker = line.indexOf(QStringLiteral("xth_me_image_insert"));
    if (marker < 0) {
        return QString();
    }

    const QString remainder = line.mid(marker + QStringLiteral("xth_me_image_insert").size()).trimmed();
    int position = 0;
    int bracedGroups = 0;
    while (position < remainder.size()) {
        while (position < remainder.size() && remainder.at(position).isSpace()) {
            ++position;
        }
        if (position >= remainder.size()) {
            break;
        }
        if (remainder.at(position) == QLatin1Char('{')) {
            int depth = 1;
            ++position;
            while (position < remainder.size() && depth > 0) {
                if (remainder.at(position) == QLatin1Char('{')) {
                    ++depth;
                } else if (remainder.at(position) == QLatin1Char('}')) {
                    --depth;
                }
                ++position;
            }
            ++bracedGroups;
            continue;
        }
        const int start = position;
        while (position < remainder.size() && !remainder.at(position).isSpace()) {
            ++position;
        }
        const QString token = remainder.mid(start, position - start).trimmed();
        if (bracedGroups >= 2 && !token.isEmpty()) {
            return token;
        }
    }
    return QString();
}

QString mapiahImagePath(const QString &line)
{
    static const QRegularExpression filenamePattern(QStringLiteral(R"(filename=([^;}]+))"));
    const QRegularExpressionMatch match = filenamePattern.match(line);
    if (!match.hasMatch()) {
        return QString();
    }
    return QUrl::fromPercentEncoding(match.captured(1).trimmed().toUtf8());
}

QVector<Th2BackgroundObject> backgroundObjectsForSourceDocument(const TherionSourceDocument &sourceDocument)
{
    QVector<Th2BackgroundObject> backgrounds;
    for (const TherionSourceDocumentLine &line : sourceDocument.lines()) {
        const QString text = line.sourceLine.text.trimmed();
        QString path;
        QString format;
        if (text.contains(QStringLiteral("##MAPIAH##"))
            && text.contains(QStringLiteral("image_insert_v1"))) {
            path = mapiahImagePath(text);
            format = QStringLiteral("mapiah");
        } else if (text.contains(QStringLiteral("##XTHERION##"))
                   && text.contains(QStringLiteral("xth_me_image_insert"))) {
            path = xtherionImagePath(text);
            format = QStringLiteral("xtherion");
        }

        if (format.isEmpty()) {
            continue;
        }

        Th2BackgroundObject background;
        background.command.kind = Th2GeometryObjectKind::Background;
        background.command.directive = QStringLiteral("background");
        background.command.sourceRange = sourceRangeForLine(line);
        background.command.id = path;
        background.command.stableKey = stableKeyForCommand(background.command);
        background.path = path;
        background.metadataFormat = format;
        backgrounds.append(background);
    }
    return backgrounds;
}

bool toggleOptionValue(const QHash<QString, QStringList> &values, const QString &name, bool fallback = false)
{
    const QString value = optionValue(values, name).trimmed().toLower();
    if (value == QStringLiteral("on") || value == QStringLiteral("true") || value == QStringLiteral("yes")) {
        return true;
    }
    if (value == QStringLiteral("off") || value == QStringLiteral("false") || value == QStringLiteral("no")) {
        return false;
    }
    return fallback;
}
}

bool Th2SourceRange::isValid() const
{
    return startLineNumber > 0 && endLineNumber >= startLineNumber && endOffset >= startOffset;
}

bool Th2SourceRange::containsLine(int lineNumber) const
{
    return isValid() && lineNumber >= startLineNumber && lineNumber <= endLineNumber;
}

bool Th2GeometryObjectRef::isValid() const
{
    return index >= 0;
}

Th2GeometryProjection Th2GeometryProjection::fromDocuments(
    const TherionSourceDocument &sourceDocument,
    const TherionSourceLogicalDocument &logicalDocument)
{
    Th2GeometryProjection projection;
    projection.backgrounds_ = backgroundObjectsForSourceDocument(sourceDocument);

    for (const TherionSourceLogicalCommand &command : logicalDocument.commands()) {
        if (command.normalizedDirective == QStringLiteral("scrap")) {
            Th2CommandObject object =
                commandObjectForLogicalCommand(Th2GeometryObjectKind::Scrap, sourceDocument, command);
            object.type = positionalToken(command, 0);
            projection.blockObjects_.append(object);
        } else if (command.normalizedDirective == QStringLiteral("map")) {
            Th2CommandObject object =
                commandObjectForLogicalCommand(Th2GeometryObjectKind::Map, sourceDocument, command);
            object.type = positionalToken(command, 0);
            projection.blockObjects_.append(object);
        } else if (command.normalizedDirective == QStringLiteral("point")
                   || command.normalizedDirective == QStringLiteral("station")) {
            Th2PointObject point;
            point.command = commandObjectForLogicalCommand(Th2GeometryObjectKind::Point, sourceDocument, command);
            const QString pointType = pointTypeToken(command);
            point.command.type = typePart(pointType);
            point.command.subtype = optionValue(point.command.optionsByName, QStringLiteral("subtype")).toLower();
            if (point.command.subtype.isEmpty()) {
                point.command.subtype = inlineSubtypePart(pointType);
            }
            if (point.command.id.isEmpty()) {
                point.command.id = optionValue(point.command.optionsByName, QStringLiteral("name"));
            }
            point.command.stableKey = stableKeyForCommand(point.command);
            const QVector<QPointF> coordinates = coordinatePointsFromLine(command.parsed);
            if (!coordinates.isEmpty()) {
                point.position = coordinates.constFirst();
                point.hasPosition = true;
            }
            projection.points_.append(point);
        } else if (command.normalizedDirective == QStringLiteral("line")) {
            Th2LineObject line;
            line.command = commandObjectForLogicalCommand(Th2GeometryObjectKind::Line, sourceDocument, command);
            fillTypeAndSubtype(&line.command, command, 0);
            line.command.stableKey = stableKeyForCommand(line.command);
            line.closed = toggleOptionValue(line.command.optionsByName, QStringLiteral("close"));
            line.pointRows = lineRowsForBlock(sourceDocument, command);
            projection.lines_.append(line);
        } else if (command.normalizedDirective == QStringLiteral("area")) {
            Th2AreaObject area;
            area.command = commandObjectForLogicalCommand(Th2GeometryObjectKind::Area, sourceDocument, command);
            fillTypeAndSubtype(&area.command, command, 0);
            area.command.stableKey = stableKeyForCommand(area.command);
            area.borderReferences = areaBorderReferencesForBlock(sourceDocument, command);
            projection.areas_.append(area);
        }
    }

    return projection;
}

Th2GeometryProjection Th2GeometryProjection::fromText(
    const QString &contents,
    const TherionSourceDocumentMetadata &metadata)
{
    TherionSourceDocumentMetadata mapMetadata = metadata;
    if (mapMetadata.sourceType == TherionSourceDocumentType::Unknown) {
        mapMetadata.sourceType = TherionSourceDocumentType::TherionMap;
    }
    const TherionSourceDocument sourceDocument = TherionSourceDocument::fromText(contents, mapMetadata);
    const TherionSourceLogicalDocument logicalDocument =
        TherionSourceLogicalDocument::fromSourceDocument(sourceDocument);
    return fromDocuments(sourceDocument, logicalDocument);
}

const QVector<Th2CommandObject> &Th2GeometryProjection::blockObjects() const
{
    return blockObjects_;
}

const QVector<Th2PointObject> &Th2GeometryProjection::points() const
{
    return points_;
}

const QVector<Th2LineObject> &Th2GeometryProjection::lines() const
{
    return lines_;
}

const QVector<Th2AreaObject> &Th2GeometryProjection::areas() const
{
    return areas_;
}

const QVector<Th2BackgroundObject> &Th2GeometryProjection::backgrounds() const
{
    return backgrounds_;
}

const Th2PointObject *Th2GeometryProjection::pointAtLineNumber(int lineNumber) const
{
    for (const Th2PointObject &point : points_) {
        if (point.command.sourceRange.containsLine(lineNumber)) {
            return &point;
        }
    }
    return nullptr;
}

const Th2LineObject *Th2GeometryProjection::lineAtLineNumber(int lineNumber) const
{
    for (const Th2LineObject &line : lines_) {
        if (line.command.sourceRange.containsLine(lineNumber)) {
            return &line;
        }
    }
    return nullptr;
}

const Th2AreaObject *Th2GeometryProjection::areaAtLineNumber(int lineNumber) const
{
    for (const Th2AreaObject &area : areas_) {
        if (area.command.sourceRange.containsLine(lineNumber)) {
            return &area;
        }
    }
    return nullptr;
}

const Th2BackgroundObject *Th2GeometryProjection::backgroundAtLineNumber(int lineNumber) const
{
    for (const Th2BackgroundObject &background : backgrounds_) {
        if (background.command.sourceRange.containsLine(lineNumber)) {
            return &background;
        }
    }
    return nullptr;
}

Th2GeometryObjectRef Th2GeometryProjection::objectRefAtLineNumber(int lineNumber) const
{
    for (int index = 0; index < points_.size(); ++index) {
        if (points_.at(index).command.sourceRange.containsLine(lineNumber)) {
            return {Th2GeometryObjectKind::Point, index};
        }
    }
    for (int index = 0; index < lines_.size(); ++index) {
        if (lines_.at(index).command.sourceRange.containsLine(lineNumber)) {
            return {Th2GeometryObjectKind::Line, index};
        }
    }
    for (int index = 0; index < areas_.size(); ++index) {
        if (areas_.at(index).command.sourceRange.containsLine(lineNumber)) {
            return {Th2GeometryObjectKind::Area, index};
        }
    }
    for (int index = 0; index < backgrounds_.size(); ++index) {
        if (backgrounds_.at(index).command.sourceRange.containsLine(lineNumber)) {
            return {Th2GeometryObjectKind::Background, index};
        }
    }
    for (int index = 0; index < blockObjects_.size(); ++index) {
        if (blockObjects_.at(index).sourceRange.containsLine(lineNumber)) {
            return {blockObjects_.at(index).kind, index};
        }
    }
    return {};
}

const Th2CommandObject *Th2GeometryProjection::commandForObjectRef(Th2GeometryObjectRef ref) const
{
    if (!ref.isValid()) {
        return nullptr;
    }
    switch (ref.kind) {
    case Th2GeometryObjectKind::Point:
        return ref.index < points_.size() ? &points_.at(ref.index).command : nullptr;
    case Th2GeometryObjectKind::Line:
        return ref.index < lines_.size() ? &lines_.at(ref.index).command : nullptr;
    case Th2GeometryObjectKind::Area:
        return ref.index < areas_.size() ? &areas_.at(ref.index).command : nullptr;
    case Th2GeometryObjectKind::Background:
        return ref.index < backgrounds_.size() ? &backgrounds_.at(ref.index).command : nullptr;
    case Th2GeometryObjectKind::Scrap:
    case Th2GeometryObjectKind::Map:
        return ref.index < blockObjects_.size() ? &blockObjects_.at(ref.index) : nullptr;
    }
    return nullptr;
}
}
