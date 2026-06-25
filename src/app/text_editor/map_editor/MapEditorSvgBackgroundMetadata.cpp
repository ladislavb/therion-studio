#include "MapEditorSvgBackgroundMetadata.h"

#include <QFile>
#include <QPointF>
#include <QRegularExpression>
#include <QXmlStreamReader>

#include <optional>

namespace TherionStudio
{
namespace
{
std::optional<qreal> parseSvgLength(QString value)
{
    value = value.trimmed();
    if (value.isEmpty() || value.endsWith(QLatin1Char('%'))) {
        return std::nullopt;
    }

    int end = 0;
    while (end < value.size()) {
        const QChar character = value.at(end);
        if (!(character.isDigit()
              || character == QLatin1Char('+')
              || character == QLatin1Char('-')
              || character == QLatin1Char('.')
              || character == QLatin1Char('e')
              || character == QLatin1Char('E'))) {
            break;
        }
        ++end;
    }

    if (end <= 0) {
        return std::nullopt;
    }

    bool ok = false;
    const qreal parsed = value.left(end).toDouble(&ok);
    if (!ok || parsed <= 0.0) {
        return std::nullopt;
    }
    return parsed;
}

QRectF parseSvgViewBox(const QString &value)
{
    const QString normalized = value.trimmed();
    if (normalized.isEmpty()) {
        return QRectF();
    }

    const QStringList tokens = normalized.split(QRegularExpression(QStringLiteral("[\\s,]+")), Qt::SkipEmptyParts);
    if (tokens.size() != 4) {
        return QRectF();
    }

    bool okLeft = false;
    bool okTop = false;
    bool okWidth = false;
    bool okHeight = false;
    const qreal left = tokens.at(0).toDouble(&okLeft);
    const qreal top = tokens.at(1).toDouble(&okTop);
    const qreal width = tokens.at(2).toDouble(&okWidth);
    const qreal height = tokens.at(3).toDouble(&okHeight);
    if (!okLeft || !okTop || !okWidth || !okHeight || width <= 0.0 || height <= 0.0) {
        return QRectF();
    }
    return QRectF(left, top, width, height);
}
}

MapEditorSvgBackgroundMetadata readMapEditorSvgBackgroundMetadata(const QString &absolutePath)
{
    QFile file(absolutePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }

    QXmlStreamReader reader(&file);
    while (!reader.atEnd()) {
        reader.readNext();
        if (!reader.isStartElement()) {
            continue;
        }
        if (reader.name().compare(QStringLiteral("svg"), Qt::CaseInsensitive) != 0) {
            continue;
        }

        const QXmlStreamAttributes attributes = reader.attributes();
        const std::optional<qreal> width = parseSvgLength(attributes.value(QStringLiteral("width")).toString());
        const std::optional<qreal> height = parseSvgLength(attributes.value(QStringLiteral("height")).toString());
        QRectF viewBox = parseSvgViewBox(attributes.value(QStringLiteral("viewBox")).toString());
        QSizeF intrinsicSize;

        if (width.has_value() && height.has_value()) {
            intrinsicSize = QSizeF(width.value(), height.value());
        } else if (viewBox.isValid()) {
            intrinsicSize = viewBox.size();
        }

        if (!viewBox.isValid() && intrinsicSize.isValid()) {
            viewBox = QRectF(QPointF(0.0, 0.0), intrinsicSize);
        }

        if (!intrinsicSize.isValid()
            || intrinsicSize.width() <= 0.0
            || intrinsicSize.height() <= 0.0
            || !viewBox.isValid()
            || viewBox.width() <= 0.0
            || viewBox.height() <= 0.0) {
            return {};
        }

        return MapEditorSvgBackgroundMetadata{intrinsicSize, viewBox, true};
    }

    return {};
}

}
