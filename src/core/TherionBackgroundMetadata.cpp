#include "TherionBackgroundMetadata.h"

#include "TherionSourceText.h"
#include "TherionStringUtils.h"

#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QUrl>
#include <cmath>

namespace TherionStudio
{
namespace
{
void skipWhitespace(const QString &text, int *position)
{
    if (position == nullptr) {
        return;
    }

    while (*position < text.size() && text.at(*position).isSpace()) {
        ++(*position);
    }
}

bool readBracedGroup(const QString &text, int *position, QString *groupText)
{
    if (position == nullptr || groupText == nullptr) {
        return false;
    }

    skipWhitespace(text, position);
    if (*position >= text.size() || text.at(*position) != QLatin1Char('{')) {
        return false;
    }

    ++(*position);
    const int contentStart = *position;
    int depth = 1;
    while (*position < text.size() && depth > 0) {
        const QChar character = text.at(*position);
        if (character == QLatin1Char('{')) {
            ++depth;
        } else if (character == QLatin1Char('}')) {
            --depth;
            if (depth == 0) {
                break;
            }
        }
        ++(*position);
    }

    if (*position >= text.size() || text.at(*position) != QLatin1Char('}')) {
        return false;
    }

    const int contentEnd = *position;
    ++(*position);
    *groupText = text.mid(contentStart, contentEnd - contentStart).trimmed();
    return true;
}

QString readSimpleToken(const QString &text, int *position)
{
    if (position == nullptr) {
        return QString();
    }

    skipWhitespace(text, position);
    if (*position >= text.size()) {
        return QString();
    }

    const QChar first = text.at(*position);
    if (first == QLatin1Char('{')) {
        QString groupText;
        if (readBracedGroup(text, position, &groupText)) {
            return groupText;
        }
        return QString();
    }

    if (first == QLatin1Char('"') || first == QLatin1Char('\'')) {
        const QChar quote = first;
        ++(*position);
        const int start = *position;
        while (*position < text.size() && text.at(*position) != quote) {
            ++(*position);
        }
        const int end = *position;
        if (*position < text.size() && text.at(*position) == quote) {
            ++(*position);
        }
        return text.mid(start, end - start).trimmed();
    }

    const int start = *position;
    while (*position < text.size() && !text.at(*position).isSpace()) {
        ++(*position);
    }
    return text.mid(start, *position - start).trimmed();
}

bool tryParseLeadingNumber(QString token, qreal *value)
{
    if (value == nullptr) {
        return false;
    }

    token = token.trimmed();
    while (!token.isEmpty()) {
        const QChar character = token.front();
        if (character.isDigit() || character == QLatin1Char('+') || character == QLatin1Char('-') || character == QLatin1Char('.')) {
            break;
        }
        token.remove(0, 1);
    }

    while (!token.isEmpty()) {
        const QChar character = token.back();
        if (character.isDigit() || character == QLatin1Char('.')) {
            break;
        }
        token.chop(1);
    }

    if (token.isEmpty()) {
        return false;
    }

    bool ok = false;
    const qreal parsed = token.toDouble(&ok);
    if (!ok) {
        return false;
    }

    *value = parsed;
    return true;
}

QString normalizeStationToken(const QString &token)
{
    QString normalized = token.trimmed();
    while (normalized.startsWith(QLatin1Char('{')) || normalized.startsWith(QLatin1Char('}'))) {
        normalized.remove(0, 1);
    }
    while (normalized.endsWith(QLatin1Char('}')) || normalized.endsWith(QLatin1Char('{'))) {
        normalized.chop(1);
    }
    return normalized.trimmed();
}

bool parseStrictNumber(const QString &token, qreal *value)
{
    if (value == nullptr) {
        return false;
    }

    bool ok = false;
    const qreal parsed = token.trimmed().toDouble(&ok);
    if (!ok) {
        return false;
    }

    *value = parsed;
    return true;
}

bool parseBooleanValue(const QString &token, bool defaultValue = false)
{
    const QString normalized = token.trimmed().toCaseFolded();
    if (normalized == QStringLiteral("true") || normalized == QStringLiteral("1") || normalized == QStringLiteral("yes")) {
        return true;
    }
    if (normalized == QStringLiteral("false") || normalized == QStringLiteral("0") || normalized == QStringLiteral("no")) {
        return false;
    }
    return defaultValue;
}

QHash<QString, QString> parseMapiahKeyValuePayload(const QString &payload)
{
    QHash<QString, QString> values;
    const QStringList entries = payload.split(QLatin1Char(';'), Qt::SkipEmptyParts);
    for (const QString &entry : entries) {
        const int separator = entry.indexOf(QLatin1Char('='));
        if (separator <= 0) {
            continue;
        }
        const QString key = entry.left(separator).trimmed();
        if (key.isEmpty()) {
            continue;
        }
        const QString encodedValue = entry.mid(separator + 1).trimmed();
        values.insert(key, QUrl::fromPercentEncoding(encodedValue.toUtf8()));
    }
    return values;
}

TherionBackgroundLayerFormat mapiahLayerFormat(const QString &format, const QString &absolutePath)
{
    const QString normalized = format.trimmed().toCaseFolded();
    if (normalized == QStringLiteral("xvi")) {
        return TherionBackgroundLayerFormat::Xvi;
    }
    if (normalized == QStringLiteral("raster")) {
        return TherionBackgroundLayerFormat::Raster;
    }
    if (normalized == QStringLiteral("svg")) {
        return TherionBackgroundLayerFormat::Svg;
    }
    if (absolutePath.endsWith(QStringLiteral(".xvi"), Qt::CaseInsensitive)) {
        return TherionBackgroundLayerFormat::Xvi;
    }
    return TherionBackgroundLayerFormat::Unsupported;
}

QString absoluteDecodedMapiahPath(const QString &filename, const QString &baseDirectory)
{
    QString decoded = filename.trimmed();
    if (decoded.isEmpty()) {
        return QString();
    }
    if (QDir::isRelativePath(decoded) && !baseDirectory.isEmpty()) {
        decoded = QDir(baseDirectory).absoluteFilePath(decoded);
    }
    return QFileInfo(decoded).absoluteFilePath();
}

QString formatTherionMetadataNumber(qreal value)
{
    if (std::fabs(value) < 1e-9) {
        return QStringLiteral("0");
    }

    const qreal nearestInteger = std::round(value);
    if (std::fabs(value - nearestInteger) < 1e-9) {
        return QString::number(static_cast<qlonglong>(nearestInteger));
    }

    return QString::number(value, 'g', 15);
}

QString mapiahLayerFormatToken(TherionBackgroundLayerFormat layerFormat)
{
    switch (layerFormat) {
    case TherionBackgroundLayerFormat::Xvi:
        return QStringLiteral("xvi");
    case TherionBackgroundLayerFormat::Svg:
        return QStringLiteral("svg");
    case TherionBackgroundLayerFormat::Raster:
    case TherionBackgroundLayerFormat::Unsupported:
        return QStringLiteral("raster");
    }
    return QStringLiteral("raster");
}

QString mapiahMetadataValue(const QString &value)
{
    return QString::fromUtf8(QUrl::toPercentEncoding(value));
}

QString mapiahRelativeFilename(const QString &absolutePath, const QString &documentPath)
{
    QString path = absolutePath;
    const QString baseDirectory = QFileInfo(documentPath).absolutePath();
    if (!baseDirectory.isEmpty()) {
        path = QDir(baseDirectory).relativeFilePath(absolutePath);
    }
    return QDir::fromNativeSeparators(path);
}

int insertionIndexAfterEncoding(const QStringList &lines)
{
    for (int index = 0; index < lines.size(); ++index) {
        QString line = lines.at(index);
        if (line.endsWith(QLatin1Char('\r'))) {
            line.chop(1);
        }
        if (line.trimmed().startsWith(QStringLiteral("encoding "))) {
            return index + 1;
        }
    }
    return 0;
}

QString upsertXtherionSimpleCommandLine(const QString &documentText,
                                        const QString &command,
                                        const QString &metadataLine)
{
    const QString lineEnding = TherionSourceText::detectedLineEnding(documentText);
    QStringList lines = TherionSourceText::splitTextLines(documentText);
    if (!lines.isEmpty() && lines.last().isEmpty()) {
        lines.removeLast();
    }

    for (int index = 0; index < lines.size(); ++index) {
        if (lines.at(index).contains(QStringLiteral("##XTHERION##")) && lines.at(index).contains(command)) {
            lines[index] = metadataLine;
            QString updated = lines.join(lineEnding);
            if (documentText.endsWith(QLatin1Char('\n')) || !updated.isEmpty()) {
                updated += lineEnding;
            }
            return updated;
        }
    }

    int insertionIndex = insertionIndexAfterEncoding(lines);
    if (command == QStringLiteral("xth_me_area_zoom_to")) {
        for (int index = 0; index < lines.size(); ++index) {
            if (lines.at(index).contains(QStringLiteral("##XTHERION##"))
                && lines.at(index).contains(QStringLiteral("xth_me_area_adjust"))) {
                insertionIndex = index + 1;
                break;
            }
        }
    }

    lines.insert(insertionIndex, metadataLine);
    QString updated = lines.join(lineEnding);
    if (documentText.endsWith(QLatin1Char('\n')) || !updated.isEmpty()) {
        updated += lineEnding;
    }
    return updated;
}
}

QVector<TherionBackgroundReference> parseTherionBackgroundReferences(const QString &documentText,
                                                                     const QString &documentPath)
{
    QVector<TherionBackgroundReference> references;
    const QString baseDirectory = QFileInfo(documentPath).absolutePath();
    const QStringList lines = splitLinesNormalizingLineEndings(documentText);
    for (int lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
        const QString trimmed = lines.at(lineIndex).trimmed();
        if (trimmed.contains(QStringLiteral("##MAPIAH##")) && trimmed.contains(QStringLiteral("image_insert_v1"))) {
            const int keywordIndex = trimmed.indexOf(QStringLiteral("image_insert_v1"));
            if (keywordIndex < 0) {
                continue;
            }

            int position = keywordIndex + QStringLiteral("image_insert_v1").size();
            QString payload;
            if (!readBracedGroup(trimmed, &position, &payload)) {
                continue;
            }
            const QHash<QString, QString> values = parseMapiahKeyValuePayload(payload);
            const QString absolutePath = absoluteDecodedMapiahPath(values.value(QStringLiteral("filename")), baseDirectory);
            if (absolutePath.isEmpty()) {
                continue;
            }

            TherionBackgroundReference reference{};
            reference.absolutePath = absolutePath;
            reference.metadataFormat = TherionBackgroundMetadataFormat::Mapiah;
            reference.layerFormat = mapiahLayerFormat(values.value(QStringLiteral("format")), absolutePath);
            reference.xviReference = reference.layerFormat == TherionBackgroundLayerFormat::Xvi;
            reference.metadataTopEdgeAnchor = reference.layerFormat == TherionBackgroundLayerFormat::Raster
                || reference.layerFormat == TherionBackgroundLayerFormat::Svg;
            reference.lineNumber = lineIndex + 1;

            qreal baseX = 0.0;
            qreal baseY = 0.0;
            if (parseStrictNumber(values.value(QStringLiteral("xx")), &baseX)
                && parseStrictNumber(values.value(QStringLiteral("yy")), &baseY)) {
                reference.basePosition = QPointF(baseX, baseY);
                reference.hasBasePosition = true;
            }
            qreal parsedValue = 0.0;
            if (parseStrictNumber(values.value(QStringLiteral("xScale")), &parsedValue) && parsedValue > 0.0) {
                reference.xScale = parsedValue;
            }
            if (parseStrictNumber(values.value(QStringLiteral("yScale")), &parsedValue) && parsedValue > 0.0) {
                reference.yScale = parsedValue;
            }
            if (parseStrictNumber(values.value(QStringLiteral("rotationCenterDx")), &parsedValue)) {
                reference.rotationCenterDx = parsedValue;
            }
            if (parseStrictNumber(values.value(QStringLiteral("rotationCenterDy")), &parsedValue)) {
                reference.rotationCenterDy = parsedValue;
            }
            if (parseStrictNumber(values.value(QStringLiteral("rotationDeg")), &parsedValue)) {
                reference.rotationDeg = parsedValue;
            }
            reference.pivotSet = parseBooleanValue(values.value(QStringLiteral("pivotSet")), false);
            if (parseStrictNumber(values.value(QStringLiteral("gamma")), &parsedValue) && parsedValue > 0.0) {
                reference.imageScale = parsedValue;
                reference.hasImageScale = true;
            }
            const QString rootStationName = normalizeStationToken(values.value(QStringLiteral("xviRoot")));
            if (!rootStationName.isEmpty() && rootStationName != QStringLiteral("0")) {
                reference.rootStationName = rootStationName;
            }
            qreal intrinsicWidth = 0.0;
            qreal intrinsicHeight = 0.0;
            if (parseStrictNumber(values.value(QStringLiteral("intrinsicWidth")), &intrinsicWidth)
                && parseStrictNumber(values.value(QStringLiteral("intrinsicHeight")), &intrinsicHeight)
                && intrinsicWidth > 0.0
                && intrinsicHeight > 0.0) {
                reference.svgIntrinsicSize = QSizeF(intrinsicWidth, intrinsicHeight);
                reference.hasSvgIntrinsicSize = true;
            }
            qreal sourceViewBoxLeft = 0.0;
            qreal sourceViewBoxTop = 0.0;
            qreal sourceViewBoxWidth = 0.0;
            qreal sourceViewBoxHeight = 0.0;
            if (parseStrictNumber(values.value(QStringLiteral("sourceViewBoxLeft")), &sourceViewBoxLeft)
                && parseStrictNumber(values.value(QStringLiteral("sourceViewBoxTop")), &sourceViewBoxTop)
                && parseStrictNumber(values.value(QStringLiteral("sourceViewBoxWidth")), &sourceViewBoxWidth)
                && parseStrictNumber(values.value(QStringLiteral("sourceViewBoxHeight")), &sourceViewBoxHeight)
                && sourceViewBoxWidth > 0.0
                && sourceViewBoxHeight > 0.0) {
                reference.svgSourceViewBox =
                    QRectF(sourceViewBoxLeft, sourceViewBoxTop, sourceViewBoxWidth, sourceViewBoxHeight);
                reference.hasSvgSourceViewBox = true;
            }
            references.append(reference);
            continue;
        }

        if (trimmed.contains(QStringLiteral("##XTHERION##")) && trimmed.contains(QStringLiteral("xth_me_image_insert"))) {
            int keywordIndex = trimmed.indexOf(QStringLiteral("xth_me_image_insert"));
            if (keywordIndex < 0) {
                continue;
            }

            int position = keywordIndex + QStringLiteral("xth_me_image_insert").size();
            QString firstGroup;
            QString secondGroup;
            if (!readBracedGroup(trimmed, &position, &firstGroup)) {
                continue;
            }
            if (!readBracedGroup(trimmed, &position, &secondGroup)) {
                secondGroup = readSimpleToken(trimmed, &position);
            }
            if (secondGroup.isEmpty()) {
                continue;
            }

            const QString imageToken = readSimpleToken(trimmed, &position);
            if (imageToken.isEmpty()) {
                continue;
            }

            QString absolutePath = imageToken;
            if (QDir::isRelativePath(absolutePath) && !baseDirectory.isEmpty()) {
                absolutePath = QDir(baseDirectory).absoluteFilePath(absolutePath);
            }
            absolutePath = QFileInfo(absolutePath).absoluteFilePath();
            if (absolutePath.isEmpty()) {
                continue;
            }

            TherionBackgroundReference reference{};
            reference.absolutePath = absolutePath;
            reference.metadataFormat = TherionBackgroundMetadataFormat::XTherion;
            reference.xviReference = reference.absolutePath.endsWith(QStringLiteral(".xvi"), Qt::CaseInsensitive);
            reference.layerFormat = reference.xviReference ? TherionBackgroundLayerFormat::Xvi : TherionBackgroundLayerFormat::Raster;
            reference.metadataTopEdgeAnchor = !reference.xviReference;
            reference.lineNumber = lineIndex + 1;

            const QStringList firstParts = tokenizeWhitespace(firstGroup);
            const QStringList secondParts = tokenizeWhitespace(secondGroup);
            if (!firstParts.isEmpty() && !secondParts.isEmpty()) {
                qreal baseX = 0.0;
                qreal baseY = 0.0;
                if (tryParseLeadingNumber(firstParts.first(), &baseX)
                    && tryParseLeadingNumber(secondParts.first(), &baseY)) {
                    reference.basePosition = QPointF(baseX, baseY);
                    reference.hasBasePosition = true;
                }
            }

            if (firstParts.size() >= 2) {
                qreal visibility = 0.0;
                if (tryParseLeadingNumber(firstParts.at(1), &visibility)) {
                    reference.visible = visibility > 0.0;
                    reference.hasVisibility = true;
                }
            }

            if (firstParts.size() >= 3) {
                qreal imageScale = 0.0;
                if (tryParseLeadingNumber(firstParts.at(2), &imageScale) && imageScale > 0.0) {
                    reference.imageScale = imageScale;
                    reference.hasImageScale = true;
                }
            }

            if (secondParts.size() >= 2) {
                const QString candidateRoot = normalizeStationToken(secondParts.at(1));
                if (!candidateRoot.isEmpty() && candidateRoot != QStringLiteral("0")) {
                    reference.rootStationName = candidateRoot;
                }
            }
            references.append(reference);
        }
    }

    return references;
}

TherionAreaAdjust parseTherionAreaAdjust(const QString &documentText)
{
    const QStringList lines = splitLinesNormalizingLineEndings(documentText);
    for (const QString &line : lines) {
        const QString trimmed = line.trimmed();
        if (!trimmed.contains(QStringLiteral("##XTHERION##")) || !trimmed.contains(QStringLiteral("xth_me_area_adjust"))) {
            continue;
        }

        const int keywordIndex = trimmed.indexOf(QStringLiteral("xth_me_area_adjust"));
        if (keywordIndex < 0) {
            continue;
        }

        const QString payload = trimmed.mid(keywordIndex + QStringLiteral("xth_me_area_adjust").size());
        const QStringList tokens = tokenizeWhitespace(payload);
        QVector<qreal> numbers;
        numbers.reserve(tokens.size());
        for (const QString &token : tokens) {
            qreal value = 0.0;
            if (tryParseLeadingNumber(token, &value)) {
                numbers.append(value);
            }
        }

        if (numbers.size() < 4) {
            continue;
        }

        const qreal left = qMin(numbers.at(0), numbers.at(2));
        const qreal right = qMax(numbers.at(0), numbers.at(2));
        const qreal bottom = qMin(numbers.at(1), numbers.at(3));
        const qreal top = qMax(numbers.at(1), numbers.at(3));
        const QRectF rect(QPointF(left, bottom), QPointF(right, top));
        if (rect.isValid() && rect.width() > 0.0 && rect.height() > 0.0) {
            return TherionAreaAdjust{rect, true};
        }
    }

    return TherionAreaAdjust{};
}

QString therionMapiahImageInsertMetadataLine(const QString &absolutePath,
                                             const QString &documentPath,
                                             TherionBackgroundLayerFormat layerFormat,
                                             const QPointF &basePosition,
                                             qreal xScale,
                                             qreal yScale,
                                             qreal rotationCenterDx,
                                             qreal rotationCenterDy,
                                             qreal rotationDeg,
                                             bool pivotSet,
                                             const QString &rootStationName,
                                             const QSizeF &svgIntrinsicSize,
                                             const QRectF &svgSourceViewBox)
{
    QStringList parts;
    parts.append(QStringLiteral("format=%1").arg(mapiahLayerFormatToken(layerFormat)));
    parts.append(QStringLiteral("filename=%1").arg(mapiahMetadataValue(mapiahRelativeFilename(absolutePath, documentPath))));
    parts.append(QStringLiteral("xx=%1").arg(mapiahMetadataValue(formatTherionMetadataNumber(basePosition.x()))));
    parts.append(QStringLiteral("yy=%1").arg(mapiahMetadataValue(formatTherionMetadataNumber(basePosition.y()))));
    parts.append(QStringLiteral("xScale=%1").arg(mapiahMetadataValue(formatTherionMetadataNumber(qMax(0.01, xScale)))));
    parts.append(QStringLiteral("yScale=%1").arg(mapiahMetadataValue(formatTherionMetadataNumber(qMax(0.01, yScale)))));
    parts.append(QStringLiteral("rotationCenterDx=%1").arg(mapiahMetadataValue(formatTherionMetadataNumber(rotationCenterDx))));
    parts.append(QStringLiteral("rotationCenterDy=%1").arg(mapiahMetadataValue(formatTherionMetadataNumber(rotationCenterDy))));
    parts.append(QStringLiteral("rotationDeg=%1").arg(mapiahMetadataValue(formatTherionMetadataNumber(rotationDeg))));
    parts.append(QStringLiteral("pivotSet=%1").arg(pivotSet ? QStringLiteral("true") : QStringLiteral("false")));
    if (layerFormat == TherionBackgroundLayerFormat::Xvi) {
        parts.append(QStringLiteral("xviRoot=%1").arg(mapiahMetadataValue(rootStationName)));
    } else if (layerFormat == TherionBackgroundLayerFormat::Svg
               && svgIntrinsicSize.isValid()
               && svgIntrinsicSize.width() > 0.0
               && svgIntrinsicSize.height() > 0.0
               && svgSourceViewBox.isValid()
               && svgSourceViewBox.width() > 0.0
               && svgSourceViewBox.height() > 0.0) {
        parts.append(QStringLiteral("intrinsicWidth=%1")
                         .arg(mapiahMetadataValue(formatTherionMetadataNumber(svgIntrinsicSize.width()))));
        parts.append(QStringLiteral("intrinsicHeight=%1")
                         .arg(mapiahMetadataValue(formatTherionMetadataNumber(svgIntrinsicSize.height()))));
        parts.append(QStringLiteral("sourceViewBoxLeft=%1")
                         .arg(mapiahMetadataValue(formatTherionMetadataNumber(svgSourceViewBox.left()))));
        parts.append(QStringLiteral("sourceViewBoxTop=%1")
                         .arg(mapiahMetadataValue(formatTherionMetadataNumber(svgSourceViewBox.top()))));
        parts.append(QStringLiteral("sourceViewBoxWidth=%1")
                         .arg(mapiahMetadataValue(formatTherionMetadataNumber(svgSourceViewBox.width()))));
        parts.append(QStringLiteral("sourceViewBoxHeight=%1")
                         .arg(mapiahMetadataValue(formatTherionMetadataNumber(svgSourceViewBox.height()))));
    }
    return QStringLiteral("##MAPIAH## image_insert_v1 {%1}").arg(parts.join(QLatin1Char(';')));
}

QString therionAreaAdjustMetadataLine(const QRectF &modelRect)
{
    const QRectF rect = modelRect.normalized();
    return QStringLiteral("##XTHERION## xth_me_area_adjust %1 %2 %3 %4")
        .arg(formatTherionMetadataNumber(rect.left()),
             formatTherionMetadataNumber(rect.top()),
             formatTherionMetadataNumber(rect.right()),
             formatTherionMetadataNumber(rect.bottom()));
}

QString therionAreaZoomToMetadataLine()
{
    return QStringLiteral("##XTHERION## xth_me_area_zoom_to 100");
}

QString upsertTherionAreaAdjustMetadata(const QString &documentText, const QRectF &modelRect)
{
    QString updated = upsertXtherionSimpleCommandLine(documentText,
                                                      QStringLiteral("xth_me_area_adjust"),
                                                      therionAreaAdjustMetadataLine(modelRect));
    updated = upsertXtherionSimpleCommandLine(updated,
                                              QStringLiteral("xth_me_area_zoom_to"),
                                              therionAreaZoomToMetadataLine());
    return updated;
}
}
