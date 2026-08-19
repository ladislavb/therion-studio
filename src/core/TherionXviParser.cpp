#include "TherionXviParser.h"

#include "TherionStringUtils.h"

#include <QFile>
#include <QtMath>

namespace TherionStudio
{
namespace
{
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

QVector<QPointF> parsePointPairs(const QStringList &tokens, int startIndex)
{
    QVector<QPointF> points;
    QVector<qreal> numbers;
    for (int index = qMax(0, startIndex); index < tokens.size(); ++index) {
        qreal value = 0.0;
        if (tryParseLeadingNumber(tokens.at(index), &value)) {
            numbers.append(value);
        }
    }

    for (int index = 0; index + 1 < numbers.size(); index += 2) {
        points.append(QPointF(numbers.at(index), numbers.at(index + 1)));
    }
    return points;
}

bool isXviGridDefinitionLine(const QString &trimmed)
{
    const QString prefix = QStringLiteral("set XVIgrid");
    if (!trimmed.startsWith(prefix)) {
        return false;
    }

    if (trimmed.size() == prefix.size()) {
        return true;
    }

    const QChar next = trimmed.at(prefix.size());
    return next.isSpace() || next == QLatin1Char('{');
}
}

bool parseTherionXviDocumentText(const QString &content, TherionXviDocument *document)
{
    if (document == nullptr) {
        return false;
    }

    *document = TherionXviDocument{};
    const QStringList lines = splitLinesNormalizingLineEndings(content);
    enum class Block
    {
        None,
        Stations,
        Shots,
        SketchLines
    };
    Block block = Block::None;

    for (const QString &line : lines) {
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }

        if (trimmed.startsWith(QStringLiteral("set XVIstations"))) {
            block = Block::Stations;
            continue;
        }
        if (trimmed.startsWith(QStringLiteral("set XVIshots"))) {
            block = Block::Shots;
            continue;
        }
        if (trimmed.startsWith(QStringLiteral("set XVIsketchlines"))) {
            block = Block::SketchLines;
            continue;
        }
        if (isXviGridDefinitionLine(trimmed)) {
            const int open = trimmed.indexOf(QLatin1Char('{'));
            const int close = trimmed.lastIndexOf(QLatin1Char('}'));
            if (open >= 0 && close > open) {
                const QString payload = trimmed.mid(open + 1, close - open - 1);
                const QStringList tokens = tokenizeWhitespace(payload);
                if (tokens.size() >= 2) {
                    qreal x = 0.0;
                    qreal y = 0.0;
                    if (tryParseLeadingNumber(tokens.at(0), &x) && tryParseLeadingNumber(tokens.at(1), &y)) {
                        document->gridOrigin = QPointF(x, y);
                        document->hasGridOrigin = true;
                    }
                }

                QVector<qreal> numbers;
                numbers.reserve(tokens.size());
                for (const QString &token : tokens) {
                    qreal value = 0.0;
                    if (tryParseLeadingNumber(token, &value)) {
                        numbers.append(value);
                    }
                }
                if (numbers.size() >= 8) {
                    document->gridVectorX = QPointF(numbers.at(2), numbers.at(3));
                    document->gridVectorY = QPointF(numbers.at(4), numbers.at(5));
                    document->gridCountX = qMax(0, qRound(numbers.at(6)));
                    document->gridCountY = qMax(0, qRound(numbers.at(7)));
                    document->hasGridDefinition = document->gridCountX > 0 && document->gridCountY > 0;
                }
            }
            block = Block::None;
            continue;
        }

        if (trimmed == QStringLiteral("}")) {
            block = Block::None;
            continue;
        }

        const int open = trimmed.indexOf(QLatin1Char('{'));
        const int close = trimmed.lastIndexOf(QLatin1Char('}'));
        if (open < 0 || close <= open) {
            continue;
        }

        const QString payload = trimmed.mid(open + 1, close - open - 1).trimmed();
        const QStringList tokens = tokenizeWhitespace(payload);
        if (tokens.isEmpty()) {
            continue;
        }

        if (block == Block::Stations) {
            if (tokens.size() < 3) {
                continue;
            }
            qreal x = 0.0;
            qreal y = 0.0;
            if (!tryParseLeadingNumber(tokens.at(0), &x) || !tryParseLeadingNumber(tokens.at(1), &y)) {
                continue;
            }
            const QString stationName = normalizeStationToken(tokens.at(2));
            if (!stationName.isEmpty()) {
                const QPointF stationPosition(x, y);
                document->stationEntries.append(TherionXviStation{stationName, stationPosition});
                if (!document->stations.contains(stationName)) {
                    // XTherion root placement scans the station table in order and uses the first matching name.
                    document->stations.insert(stationName, stationPosition);
                }
            }
            continue;
        }

        if (block == Block::Shots) {
            const QVector<QPointF> points = parsePointPairs(tokens, 0);
            if (points.size() >= 2) {
                TherionXviShot shot;
                shot.centerLine = QLineF(points.at(0), points.at(1));
                // XTherion draws the first four points after the centreline as a
                // passage quadrilateral whenever the record has 12 values.
                if (points.size() >= 6) {
                    shot.passageOutline.reserve(4);
                    for (int index = 2; index < 6; ++index) {
                        shot.passageOutline.append(points.at(index));
                    }
                }
                document->shots.append(shot);
            }
            continue;
        }

        if (block == Block::SketchLines) {
            const QVector<QPointF> points = parsePointPairs(tokens, 1);
            if (points.size() >= 2) {
                TherionXviSketchLine sketchLine;
                sketchLine.colorToken = tokens.first().trimmed();
                sketchLine.points = points;
                document->sketchLines.append(sketchLine);
            }
            continue;
        }
    }

    return document->hasGridOrigin
        && (!document->shots.isEmpty() || !document->sketchLines.isEmpty() || !document->stations.isEmpty());
}

bool parseTherionXviDocumentFile(const QString &xviPath, TherionXviDocument *document)
{
    if (document == nullptr) {
        return false;
    }

    QFile file(xviPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    return parseTherionXviDocumentText(QString::fromUtf8(file.readAll()), document);
}
}
